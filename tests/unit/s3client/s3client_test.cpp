#include "s3client/s3client.hpp"

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <gmock/gmock.h>
#include <userver/engine/async.hpp>
#include <userver/engine/exception.hpp>
#include <userver/engine/task/cancel.hpp>
#include <userver/engine/task/task_with_result.hpp>
#include <userver/s3api/utest/client_gmock.hpp>
#include <userver/utest/utest.hpp>

namespace RumpelQuiz {
namespace {
using MockClient = testing::StrictMock<userver::s3api::GMockClient>;
using testing::_;
const S3ClientConfig kConfig{"storage.example.com", "quiz-images"};

void ExpectFailure(const SaveImageResponse& result) {
  EXPECT_FALSE(result.success);
  EXPECT_FALSE(result.error_message.empty());
  EXPECT_TRUE(result.url.empty());
}

UTEST(S3Client, PreservesMultipartBytesKeyAndMimeType) {
  auto mock = std::make_shared<MockClient>();
  S3Client client(mock, kConfig);
  const std::string contents{"\xff\0\xd8\0", 4};
  EXPECT_CALL(*mock, PutObject("images/123.jpg", contents, testing::Eq(std::nullopt),
                              "image/jpeg", testing::Eq(std::nullopt), testing::Eq(std::nullopt)))
      .WillOnce(testing::Return("etag"));
  const S3ClientBase& storage = client;
  const auto result = storage.SaveImage(contents, "images/123.jpg", "image/jpeg");
  EXPECT_TRUE(result.success);
  EXPECT_TRUE(result.error_message.empty());
  EXPECT_EQ(result.url, "https://quiz-images.storage.example.com/images/123.jpg");
}

UTEST(S3Client, StorageFailureHasSafeError) {
  auto mock = std::make_shared<MockClient>();
  S3Client client(mock, kConfig);
  EXPECT_CALL(*mock, PutObject(_, _, _, _, _, _))
      .WillOnce(testing::Throw(std::runtime_error("sensitive transport details")));
  const auto result = client.SaveImage("bytes", "images/x.png", "image/png");
  ExpectFailure(result);
  EXPECT_EQ(result.error_message, "S3 image upload failed");
}

UTEST(S3Client, SupportsAllCanonicalImageTypes) {
  auto mock = std::make_shared<MockClient>();
  S3Client client(mock, kConfig);
  for (const auto type : {"image/jpeg", "image/png", "image/webp", "image/gif"}) {
    SCOPED_TRACE(type);
    EXPECT_CALL(*mock, PutObject("images/object", "bytes", _, type, _, _))
        .WillOnce(testing::Return("etag"));
    EXPECT_TRUE(client.SaveImage("bytes", "images/object", type).success);
  }
}

UTEST(S3Client, PreservesStringViewBoundaries) {
  auto mock = std::make_shared<MockClient>();
  S3Client client(mock, kConfig);
  const std::string key = "!images/test.png?ignored";
  const std::string contents{"!a\0b?ignored", 12};
  const std::string type = "!image/png?ignored";
  EXPECT_CALL(*mock, PutObject("images/test.png", std::string("a\0b", 3), _, "image/png", _, _))
      .WillOnce(testing::Return("etag"));
  const auto result = client.SaveImage(std::string_view(contents).substr(1, 3),
                                      std::string_view(key).substr(1, 15),
                                      std::string_view(type).substr(1, 9));
  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.url, "https://quiz-images.storage.example.com/images/test.png");
}

UTEST(S3Client, AcceptsMaximumLengthKeyAndSafeCharacters) {
  auto mock = std::make_shared<MockClient>();
  S3Client client(mock, kConfig);
  std::string key = "images/AZaz09-_./";
  key.resize(1024, 'a');
  EXPECT_CALL(*mock, PutObject(key, "bytes", _, "image/png", _, _))
      .WillOnce(testing::Return("etag"));
  EXPECT_TRUE(client.SaveImage("bytes", key, "image/png").success);
}

UTEST(S3Client, FailedUploadCanBeRetriedAndSameKeyCanBeReplaced) {
  auto mock = std::make_shared<MockClient>();
  S3Client client(mock, kConfig);
  testing::InSequence sequence;
  EXPECT_CALL(*mock, PutObject("images/x.png", "first", _, "image/png", _, _))
      .WillOnce(testing::Throw(std::runtime_error("storage unavailable")));
  EXPECT_CALL(*mock, PutObject("images/x.png", "first", _, "image/png", _, _))
      .WillOnce(testing::Return("etag-1"));
  EXPECT_CALL(*mock, PutObject("images/x.png", "replacement", _, "image/png", _, _))
      .WillOnce(testing::Return("etag-2"));
  ExpectFailure(client.SaveImage("first", "images/x.png", "image/png"));
  EXPECT_TRUE(client.SaveImage("first", "images/x.png", "image/png").success);
  EXPECT_TRUE(client.SaveImage("replacement", "images/x.png", "image/png").success);
}

UTEST(S3Client, CancellationPropagatesInsteadOfStorageError) {
  for (const bool transport_throws : {false, true}) {
    SCOPED_TRACE(transport_throws);
    auto mock = std::make_shared<MockClient>();
    S3Client client(mock, kConfig);
    EXPECT_CALL(*mock, PutObject(_, _, _, _, _, _))
        .WillOnce(testing::InvokeWithoutArgs([transport_throws]() -> std::string {
          userver::engine::current_task::RequestCancel();
          if (transport_throws) throw std::runtime_error("transport interrupted");
          userver::engine::current_task::CancellationPoint();
          return "unreachable";
        }));
    auto task = userver::engine::AsyncNoTracing([&client] {
      return client.SaveImage("bytes", "images/x.png", "image/png");
    });
    EXPECT_THROW(task.Get(), userver::engine::TaskCancelledException);
  }
}

UTEST(S3Client, RejectsInvalidInputBeforeUpload) {
  auto mock = std::make_shared<MockClient>();
  S3Client client(mock, kConfig);
  for (const auto key : {"", "/image.png", "images/../image.png", "images//x.png",
       "images/x#1.png", "images/x?1.png", "images/", ".", "..", "images/./x.png",
       "images/.", "images/..", "images/x\\y.png", "images/x%2fy.png", "images/x y.png",
       "images/x\r\ny.png"}) {
    SCOPED_TRACE(key);
    ExpectFailure(client.SaveImage("bytes", key, "image/png"));
  }
  ExpectFailure(client.SaveImage("bytes", std::string("images/x\0.png", 13), "image/png"));
  ExpectFailure(client.SaveImage("bytes", std::string(1025, 'a'), "image/png"));
  ExpectFailure(client.SaveImage("", "images/x.png", "image/png"));
  for (const auto type : {"", "png", "image/jpg", "image/svg+xml", "image/png\r\nX-Test: x"}) {
    ExpectFailure(client.SaveImage("bytes", "images/x.png", type));
  }
}

UTEST(S3Client, DisabledStorageRejectsUpload) {
  S3Client client;
  ExpectFailure(client.SaveImage("bytes", "images/x.png", "image/png"));
}

UTEST(S3Client, NullDependencyIsRejected) {
  EXPECT_THROW((S3Client{nullptr, kConfig}), std::invalid_argument);
}

UTEST(S3Client, RejectsInvalidUrlConfiguration) {
  auto mock = std::make_shared<MockClient>();
  for (const auto host : {"", "https://storage.example.com", "host/path", "host?query",
                          "host@other", "host\r\n", ".host", "host..name"}) {
    SCOPED_TRACE(host);
    EXPECT_THROW((S3Client{mock, {host, "bucket"}}), std::invalid_argument);
    EXPECT_THROW((S3Client{mock, {"storage.example.com", host}}), std::invalid_argument);
  }
}
UTEST(S3Client, ReadsBinaryObjectAndDistinguishesMissingFromFailure) {
  auto mock = std::make_shared<MockClient>();
  S3Client client(mock, kConfig);
  const std::string bytes{"a\0b", 3};
  EXPECT_CALL(*mock, GetObject("images/example.png", _, _, _))
      .WillOnce(testing::Return(std::optional<std::string>{bytes}))
      .WillOnce(testing::Return(std::nullopt))
      .WillOnce(testing::Throw(std::runtime_error("secret transport details")));
  const auto found = client.LoadImage("images/example.png");
  EXPECT_TRUE(found.success);
  EXPECT_TRUE(found.found);
  EXPECT_EQ(found.contents, bytes);
  const auto missing = client.LoadImage("images/example.png");
  EXPECT_TRUE(missing.success);
  EXPECT_FALSE(missing.found);
  const auto failed = client.LoadImage("images/example.png");
  EXPECT_FALSE(failed.success);
  EXPECT_TRUE(failed.contents.empty());
  EXPECT_FALSE(client.LoadImage("../secret").success);
  EXPECT_FALSE(S3Client{}.LoadImage("images/example.png").success);
}

}  // namespace
}  // namespace RumpelQuiz
