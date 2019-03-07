# libevlite性能测试



### 一、吞吐量测试结果

##### 1. 硬件配置

2CPU4Core

CPU型号 Intel(R) Xeon(R) CPU E5606 @ 2.13GHz

8G内存

Ubuntu 14.04.5

##### 2. 4K数据包，4个IO线程

| 连接数    | 1000    | 2000    | 10000   |
| --------- | ------- | ------- | ------- |
| muduo     | 197.515 | 232.825 | 244.675 |
| libevlite | 245.582 | 238.293 | 244.478 |

##### 3. 16K数据包，4个IO线程

| 连接数    | 1000    | 2000    | 10000   |
| --------- | ------- | ------- | ------- |
| muduo     | 596.692 | 598.942 | 588.075 |
| libevlite | 630.339 | 598.021 | 571.337 |