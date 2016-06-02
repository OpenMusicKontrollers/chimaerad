#!/bin/sh

URL='http://localhost:8080'
OS=$(uname -s)

case "${OS}" in
	Darwin)
		OPEN='open' ;;
	*)
		OPEN='xdg-open' ;;
esac

sleep 1 && ${OPEN} ${URL} &
exec ./chimaerad app.zip
