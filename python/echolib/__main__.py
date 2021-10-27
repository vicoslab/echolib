
import sys

def main():
    from echolib import router
    try:
        if len(sys.argv) > 1:
            router(sys.argv[1])
        else:
            router()
    except KeyboardInterrupt:
        pass

if __name__ == "__main__":
    main()
    