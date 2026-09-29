#include "media_service.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <variant>

#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/options.hpp>

#include "media/service/image_validation.hpp"
#include "s3client/s3client_component.hpp"

namespace RumpelQuiz {

MediaService::MediaService(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context
)
    : ComponentBase(config, context),
      pg_(
          context.FindComponent<userver::components::Postgres>(
              "postgres-db-1"
          ).GetCluster()
      ),
      s3_client_(
          context.FindComponent<S3ClientComponent>().GetClient()
      ) {}

UploadImageResult MediaService::UploadImage(
    const boost::uuids::uuid& user_id,
    std::string_view contents
) const {
    // 1. Проверяем, есть ли активная роль преподавателя.
    {
        auto transaction = pg_->Begin(
            userver::storages::postgres::ClusterHostType::kMaster,
            userver::storages::postgres::TransactionOptions{}
        );

        const auto memberships =
            education_repository_.GetAllActiveMembershipsByUserId(
                transaction,
                user_id
            );

        const bool canUpload = std::any_of(
            memberships.begin(),
            memberships.end(),
            [](const Membership& membership) {
                return membership.role == "teacher"
                    || membership.role == "admin";
            }
        );

        transaction.Commit();

        if (!canUpload) {
            return UploadImageError::kAccessDenied;
        }
    }

    // 2. Проверяем содержимое изображения.
    const auto validation = ValidateImage(contents);

    if (const auto* error =
            std::get_if<UploadImageError>(&validation)) {
        return *error;
    }

    const auto& image = std::get<ImageInfo>(validation);

    // Имя файла пользователя не используем как ключ S3.
    const auto media_id = boost::uuids::random_generator{}();

    const std::string storage_key =
        "images/" +
        boost::uuids::to_string(media_id) +
        "." +
        image.extension;

    // 3. Сначала регистрируем намерение загрузить файл.
    {
        auto transaction = pg_->Begin(
            userver::storages::postgres::ClusterHostType::kMaster,
            userver::storages::postgres::TransactionOptions{}
        );

        media_repository_.CreatePending(
            transaction,
            MediaRecord{
                media_id,
                user_id,
                storage_key,
                image.content_type,
                static_cast<std::int64_t>(contents.size()),
                image.width,
                image.height
            }
        );

        transaction.Commit();
    }

    // 4. Отправляем файл в S3 без открытой транзакции БД.
    const auto saved = s3_client_.SaveImage(contents, storage_key, image.content_type);
    if (!saved.success) {
        // Не логируем секреты, подписанные URL и содержимое файла.
        LOG_WARNING()
            << "Image upload failed, media_id="
            << boost::uuids::to_string(media_id);

        // Запись остаётся pending для последующей очистки.
        return UploadImageError::kStorageUnavailable;
    }

    // 5. Только после успешного ответа S3 разрешаем
    // использовать изображение в квизе.
    {
        auto transaction = pg_->Begin(
            userver::storages::postgres::ClusterHostType::kMaster,
            userver::storages::postgres::TransactionOptions{}
        );

        media_repository_.MarkReady(transaction, media_id);

        transaction.Commit();
    }

    return UploadedImage{media_id, s3_client_.DownloadUrl(storage_key)};
}

}  // namespace RumpelQuiz
