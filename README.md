# over_frequency
 A simple module for OpenSIPS to control caller or callee call over frequency。Used as an example to explain the OpenSIPS module development。

See more Welcome attention WeChat official account “Code2Fun”.

该模块是我写的《OpenSIPS实战（OpenSIPS in action）》系列公众号文章中的第七篇《OpenSIPS实战（七）：模块开发-呼叫超频控制模块》中用于讲解模块开发过程而开发的模块，主要用于讲解OpenSIPS模块开发过程。

主要功能：
1、控制主叫外呼频率
2、控制被叫被呼叫频率
该模块还很不完善，只用于模块开发讲解，不建议用于实际项目中。

编译安装：
在OpenSIPS源码目录中的modules目录中新建over_frequency目录，将该模块下载到该目录，执行make命令即可编译，然后将生成的over_frequency.so文件复制到OpenSIPS安装目录的slib/opensips/modules目录中即可完成安装。


脚本中调用：

```c
loadmodule "cachedb_local.so"

loadmodule "over_frequency.so"
modparam("over_frequency", "max_frequency", 10)
modparam("over_frequency", "time_interval", 600)

route{
    ...
    if (is_method("INVITE")) {
        if (!check_frequency("1")){
            send_reply("403","Over Frequency");
            exit;
        }
    }
    ...
}
