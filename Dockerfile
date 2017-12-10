# build container
FROM python:3.6-alpine as builder

WORKDIR /build
COPY requirements.txt /build/requirements.txt
RUN pip install -r requirements.txt

COPY . /build

RUN python translator.py --compile --render


# deployment container
FROM nginx:1.13-alpine

COPY docker/nginx.conf /etc/nginx/
COPY --from=builder /build/www /usr/share/nginx/html
