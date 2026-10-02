import pytest


@pytest.mark.parametrize('path,content_type,marker', [
    ('/', 'text/html', 'RumpelQuiz'),
    ('/styles.css', 'text/css', '@media'),
    ('/app.js', 'text/javascript', 'createController'),
    ('/game.js', 'text/javascript', 'createGameController'),
    ('/vendor/qrcode.js', 'text/javascript', 'qrcode'),
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
    for path in ['/configs/config_vars.yaml', '/.env', '/frontend/app.js', '/unknown']:
        assert (await service_client.get(path)).status == 404
