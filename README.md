# RumpelQuiz

Template of a C++ service that uses [userver framework](https://github.com/userver-framework/userver).


## Local development PostgreSQL

The persistent development database is separate from the temporary PostgreSQL
instance created by testsuite. Docker Compose stores its data in the named
volume `rumpelquiz-postgres-data` and initializes it from the existing files in
`postgresql/schemas` and `postgresql/data` on the first start.

Port `5433` is used on the Windows host because the standard port `5432` is
already occupied. Start the database from PowerShell in the project directory:

```console
docker compose up -d --wait postgres
docker compose ps
```

Open the Dev Container and start the backend there:

```console
make build-debug
make start-debug
```

`make start-debug` starts the binary directly with `configs/config_vars.yaml`.
The PostgreSQL connection can be overridden without changing C++ code:

```console
DB_CONNECTION='postgresql://rumpelquiz:rumpelquiz_dev@host.docker.internal:5433/rumpelquiz' make start-debug
```

The default development connection is:

| Setting | Value |
| --- | --- |
| Host from Windows / pgAdmin | `localhost` |
| Host from the Dev Container | `host.docker.internal` |
| Port | `5433` |
| Database | `rumpelquiz` |
| Username | `rumpelquiz` |
| Password | `rumpelquiz_dev` |

Connection checks:

```console
docker compose exec postgres pg_isready -U rumpelquiz -d rumpelquiz
docker compose exec postgres psql -U rumpelquiz -d rumpelquiz -c "SELECT current_database(), current_user;"
curl http://localhost:8080/ping
curl http://localhost:8080/hello
curl http://localhost:8080/hello-postgres
curl 'http://localhost:8080/hello-postgres?name=DevCheck'
```

Stop PostgreSQL without deleting development data:

```console
docker compose down
```

To intentionally reset the development database, run `docker compose down -v`
and start it again. This deletes the development volume; it does not affect the
separate testsuite database.


## Download and Build

To create your own userver-based service follow the following steps:

1. Press the "Use this template button" at the top right of this GitHub page
2. Clone the service `git clone your-service-repo && cd your-service-repo && git submodule update --init`
3. Give a proper name to your service and replace all the occurrences of "RumpelQuiz" string with that name
4. Feel free to tweak, adjust or fully rewrite the source code of your service.


## Makefile

`PRESET` is either `debug`, `release`, or if you've added custom presets in `CMakeUserPresets.json`, it
can also be `debug-custom`, `release-custom`.

* `make cmake-PRESET` - run cmake configure, update cmake options and source file lists
* `make build-PRESET` - build the service
* `make test-PRESET` - build the service and run all tests
* `make start-PRESET` - build the service, start it in testsuite environment and leave it running
* `make install-PRESET` - build the service and install it in directory set in environment `PREFIX`
* `make` or `make all` - build and run all tests in `debug` and `release` modes
* `make format` - reformat all C++ and Python sources
* `make dist-clean` - clean build files and cmake cache
* `make docker-COMMAND` - run `make COMMAND` in docker environment
* `make docker-clean-data` - stop docker containers


## License

The original template is distributed under the [Apache-2.0 License](https://github.com/userver-framework/userver/blob/v3.1/LICENSE)
and [CLA](https://github.com/userver-framework/userver/blob/v3.1/CONTRIBUTING.md). Services based on the template may change
the license and CLA.
