
# Proof-of-concept test for different Python version communication

echorouter

docker run -ti --rm -v  "`realpath tests/python2/script.py`:/script.py:ro" -v /tmp/echo.sock:/tmp/echo.sock echolib:old python script.py a b

docker run -ti --rm -v  "`realpath tests/python2/script.py`:/script.py:ro" -v /tmp/echo.sock:/tmp/echo.sock echolib python script.py b a