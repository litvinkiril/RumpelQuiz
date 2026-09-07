import os
import pytest

from testsuite.databases.pgsql import discover

pytest_plugins = [
    'pytest_userver.plugins.core',
    'pytest_userver.plugins.postgresql',
]

USERVER_CONFIG_HOOKS = ['postbox_mock_url']


@pytest.fixture(scope='session')
def service_env():
    return {
        'JWT_SECRET': 'testsuite-jwt-secret-that-is-at-least-32-characters',
    }


@pytest.fixture(scope='session')
def postbox_mock_url(mockserver_info):
    def patch_config(config_yaml, config_vars):
        components = config_yaml['components_manager']['components']
        if os.environ.get('RUMPELQUIZ_TEST_CONSOLE') == '1':
            components['email-service']['send-enabled'] = False
            components['email-service']['log-codes'] = True
            components['postbox-client']['key-id'] = ''
            components['postbox-client']['secret-key'] = ''
            components['postbox-client']['from-email'] = ''
        components['postbox-client']['url'] = mockserver_info.url(
            'v2/email/outbound-emails',
        )

    return patch_config






@pytest.fixture(scope='session')
def initial_data_path(service_source_dir):
    """Path for find files with data"""
    return [
        service_source_dir / 'postgresql/data',
    ]


@pytest.fixture(scope='session')
def pgsql_local(service_source_dir, pgsql_local_create):
    """Create schemas databases for tests"""
    databases = discover.find_schemas(
        'RumpelQuiz',
        [service_source_dir.joinpath('postgresql/schemas')],
    )
    return pgsql_local_create(list(databases.values()))
