Tri-prong
=========

HTTP benchmarking test tool achieving high precise RPS (requests per second)

FAQs
---------

#### Tri-prong的使用方法如何？

答：使用方法已经简单到不能再简单，只有三个参数，分别是-r（自己设定的RPS，即压测频率），-t（压测时间），-u（待压测的URL链接）。Tri-prong为linux命令行工具，下载后直接make，出现可执行文件tri-prong，详见Example。

#### Tri-prong与Apache自带的AB有什么区别？

答：Apache无法支持预先设定RPS的压测，只能规定并发量。而Tri-prong可以根据预先设定的RPS精准均匀地进行压测。这里“精准”的含义是，如果设定了1000RPS，那一定是将1秒时间平均分成1000段，每段压测一次，即0至1毫秒发送一次访问请求，1至2毫秒发送一次访问请求，依次类推。

#### Tri-prong真的可以做到完全按照我设定的RPS进行压测吗？

答：正常情况下，该工具一定会试图完全按照预先设定的RPS进行压测，目前代码中设置的最大RPS限制为10000，为了保证真的能达到预先设定的RPS，在程序中有实际压测频率的检测环节，如果实际RPS没有达到预先设定值，会在结果输出时给出提示。到底能否真的达到预先设定的RPS，取决于以下几个方面：
（1）机器的性能，经过实际运行，我们发现在一台性能较差的虚拟机上运行Tri-prong，受限于机器的性能，设定RPS较大时没办法达到。
（2）Tri-prong采用的是TCP短连接操作，因此需要在短时间内打开大量的socket描述符，请通过ulimit -n命令查看可以打开的文件描述符限制，必要时要使用ulimit -n 20000调大。
（3）可用的端口号范围。可以通过命令cat /proc/sys/net/ipv4/ip_local_port_range来查看，因为每次压测都要占用一个端口，RPS值比较大的时候，可能会出现端口被占满的情况，这也会限制tri-prong的最大RPS。

#### 如果设定的RPS超过了Web服务器的处理能力，会怎样？

答：这种情况下会出现超时，Tri-prong默认超过3秒没有得到HTTP响应，则认为是超时。最终的结果输出中，我们也有超时统计。需要注意的是，在超时比较多的情况下，比较容易出现errno=24错误，即打开的socket描述符过多，需要使用ulimit -n 20000调大。

Example
---------

    horstxu@horstxu:~/$ ./tri-prong -r 1000 -t 10 -u http://127.0.0.1:8080/index.html
    
    This is Tri-prong 1.1 (last updated on Dec. 23rd, 2014), an HTTP benchmarking tool.
    Copyright @ 2014 Horst Xu. Email: 271021733@qq.com
    
    Test Information
        Host       : 127.0.0.1
        Port       : 8080
        URI        : /index.html
        Target RPS : 1000   [#/second]
        Test Time  : 10     [second]
        
    Now the host is under test. Please wait a moment ...
    
    The actual RPS is 1000, which is exactly equal to your settings.
    Please refer to the test results as follows:
    
        TCP Connect Fail Count:    0         [#]
        HTTP Response Fail Count:  0         [#]
        Overall Fail Count:        0         [#]
        Total Request Number:      10000     [#]
        Fail Percent:              0.000%    
        Total Bytes Transferred:   3100000   [Bytes]
        
    Request Time  [unit: us]
                                min         max        avg
        TCP Connect Time:       147        4439        229
        HTTP Response Time:     274        3796        600
        Total Request Time:     456        5172        831
    horstxu@horstxu:~/$
