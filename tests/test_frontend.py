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
    ('/assets/features/quizzes/preview.js', 'text/javascript', 'renderQuizPreview'),
    ('/assets/features/user-creation/controller.js', 'text/javascript', 'createUserCreationController'),
    ('/assets/features/user-creation/view.js', 'text/javascript', 'Создать пользователя'),
    ('/assets/styles/users.css', 'text/css', 'user-create-form'),
    ('/assets/styles/home.css', 'text/css', '@media'),
])
async def test_frontend_assets(service_client, path, content_type, marker):
    response = await service_client.get(path)
    assert response.status == 200
    assert response.headers['Content-Type'].startswith(content_type)
    assert marker in response.text
    assert response.headers['X-Content-Type-Options'] == 'nosniff'
    assert response.headers['Cache-Control'] == 'no-store'
    assert "script-src 'self'" in response.headers['Content-Security-Policy']


async def test_frontend_does_not_expose_source_or_config(service_client):
    for path in ['/configs/config_vars.yaml', '/.env', '/frontend/app.js', '/unknown',
                 '/assets/unknown.js', '/assets/package.json', '/assets/tests/auth.test.js',
                 '/assets/.env']:
        assert (await service_client.get(path)).status == 404
