

from echolib import Client, IOLoop

import numpy as np

data = np.random.random((2, 2))

def test(t):
    np.testing.assert_array_equal(t, data, verbose=True)

def main():
    from echolib.array import TensorPublisher, TensorSubscriber

    loop = IOLoop()
    client = Client()
    loop.add_handler(client)

    pub1 = TensorPublisher(client, "tensor1")
    pub2 = TensorPublisher(client, "tensor2")

    sub1 = TensorSubscriber(client, "tensor1", lambda t: pub2.send(t) or test(t))
    sub2 = TensorSubscriber(client, "tensor2", lambda t: test(t))

    loop.wait(10)
    pub1.send(data)
    loop.wait(100)

    client.disconnect()

if __name__ == "__main__":
    main()