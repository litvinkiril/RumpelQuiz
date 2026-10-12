import pytest


@pytest.mark.parametrize('path,content_type,marker', [
    ('/', 'text/html', 'RumpelQuiz'),
    ('/styles.css', 'text/css', '@import'),
    ('/app.js', 'text/javascript', 'createController'),
    ('/test.js', 'text/javascript', 'createTestController'),
    ('/game.js', 'text/javascript', 'createGameController'),
    ('/vendor/qrcode.js', 'text/javascript', 'qrcode'),
    ('/assets/core/controller.js', 'text/javascript', 'createController'),
    ('/assets/features/profile/view.js', 'text/javascript', 'Описание'),
    ('/assets/features/education/controller.js', 'text/javascript', 'openEducationCatalog'),
    ('/assets/features/education/catalog.js', 'text/javascript', 'renderEducationCatalog'),
    ('/assets/features/education/search.js', 'text/javascript', 'createEducationSearchController'),
    ('/assets/features/education/search-view.js', 'text/javascript', 'renderEducationSearch'),
    ('/assets/features/education/creation.js', 'text/javascript', 'createEducationCreationController'),
    ('/assets/features/education/creation-view.js', 'text/javascript', 'renderEducationCreation'),
    ('/assets/features/education/creation-model.js', 'text/javascript', 'canCreateEducation'),
    ('/assets/features/quizzes/preview.js', 'text/javascript', 'renderQuizPreview'),
    ('/assets/features/user-creation/controller.js', 'text/javascript', 'createUserCreationController'),
    ('/assets/features/user-creation/view.js', 'text/javascript', 'Создать пользователя'),
    ('/assets/styles/users.css', 'text/css', 'user-create-form'),
    ('/assets/styles/home.css', 'text/css', '@media'),
])
async def test_frontend_assets(service_client, service_source_dir, path, content_type, marker):
    response = await service_client.get(path)
    assert response.status == 200
    assert response.headers['Content-Type'].startswith(content_type)
    assert marker in response.text
    asset_path = 'index.html' if path == '/' else path.lstrip('/')
    assert response.text == (service_source_dir / 'frontend' / asset_path).read_text(encoding='utf-8')
    assert response.headers['X-Content-Type-Options'] == 'nosniff'
    assert response.headers['Cache-Control'] == 'no-store'
    assert "script-src 'self'" in response.headers['Content-Security-Policy']


async def test_frontend_does_not_expose_source_or_config(service_client):
    for path in ['/configs/config_vars.yaml', '/.env', '/frontend/app.js', '/unknown',
                 '/assets/unknown.js', '/assets/package.json', '/assets/tests/auth.test.js',
                 '/assets/.env']:
        assert (await service_client.get(path)).status == 404
