#pragma once

#include <string_view>

#include <boost/uuid/uuid.hpp>

#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

#include "education/repository/education_repository.hpp"
#include "media/media_models.hpp"
#include "media/repository/media_repository.hpp"
#include "s3client/s3client_base.hpp"

namespace RumpelQuiz {

class MediaService final : public userver::components::ComponentBase {
public:
    static constexpr std::string_view kName = "media-service";

    MediaService(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context
    );

    UploadImageResult UploadImage(
        const boost::uuids::uuid& user_id,
        std::string_view contents
    ) const;

private:
    userver::storages::postgres::ClusterPtr pg_;

    EducationRepository education_repository_;
    MediaRepository media_repository_;
    const S3ClientBase& s3_client_;
};

}  // namespace RumpelQuiz
