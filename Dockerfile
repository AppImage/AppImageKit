# build container
FROM python:3.6-alpine as builder

WORKDIR /build
COPY requirements.txt /build/requirements.txt
RUN pip install -r requirements.txt

COPY locale/ /build/locale/
COPY www/ /build/www/
COPY *.py /build/
COPY *.jinja2 /build/

RUN python translator.py --compile --render


# deployment container
FROM nginx:1.13-alpine

COPY docker/nginx.conf /etc/nginx/

# check nginx config
RUN nginx -t

COPY --from=builder /build/www /usr/share/nginx/html
