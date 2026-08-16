usage:
usage: main -v <test> -l <location> -H <host>
tests: l - find location, h - find host, u - upload speed test, d - download speed test, a - upload and download speed test
example: main -v l
example: main -v u -l Lithuania\n
example: main -v a -H speed-kaunas.telia.lt

for help:
main -?

Dependencies:

 - curl/libcurl/lcurl
 - cjson