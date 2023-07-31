# build container
FROM python:3-alpine as builder

RUN apk add --no-cache gcc musl-dev libffi-dev rust cargo openssl-dev

# build as a regular user, not root, to avoid annoying warnings from pip
RUN adduser -S build
RUN install -d -m 0755 -o build /build
USER build
WORKDIR /build

COPY pyproject.toml /build/
COPY poetry.lock /build/
RUN pip install -U pip && \
    pip install poetry && \
    python3 -m poetry install

COPY --chown=build:nobody locale/ /build/locale/
COPY --chown=build:nobody www/ /build/www/
COPY *.py /build/
COPY *.jinja2 /build/

RUN python3 -m poetry run python translator.py --compile --render


# deployment container
FROM nginx:1-alpine

COPY docker/nginx.conf /etc/nginx/

# check nginx config
RUN nginx -t

COPY --from=builder /build/www /usr/share/nginx/html
