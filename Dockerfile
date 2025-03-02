# build container
FROM python:3-alpine as builder

RUN apk add --no-cache gcc musl-dev libffi-dev rust cargo openssl-dev poetry

# build as a regular user, not root, to avoid annoying warnings from pip
RUN adduser -S build
RUN install -d -m 0755 -o build /build
USER build
WORKDIR /build

COPY pyproject.toml /build/
COPY poetry.lock /build/

RUN poetry install

COPY --chown=build:nobody locale/ /build/locale/
COPY --chown=build:nobody www/ /build/www/
COPY *.py /build/
COPY *.jinja2 /build/

RUN poetry run python translator.py --compile --render


# deployment container
FROM nginx:1-alpine

LABEL org.opencontainers.image.source="https://github.com/AppImage/AppImageKit"

COPY docker/nginx.conf /etc/nginx/

# check nginx config
RUN nginx -t

COPY --from=builder /build/www /usr/share/nginx/html
