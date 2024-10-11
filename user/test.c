#include <unistd.h>

int main() {
    int p[2];        /* 存储管道的两个文件描述符 */
    char *argv[2];
    argv[0] = "wc";  /* 设置要执行的命令为"wc" */
    argv[1] = 0;     /* 参数数组以NULL结尾，表示没有更多参数 */
    pipe(p);         /* 创建管道，p[0]是管道的读端，p[1]是管道的写端 */

    if (fork() == 0) {
        /* 子进程 */
        close(0);    /* 关闭标准输入（文件描述符0） */
        dup(p[0]);   /* 复制管道的读端p[0]，让文件描述符0指向管道的读端 */
        close(p[0]); /* 关闭重复的管道读端 */
        // close(p[1]); /* 关闭不再需要的管道写端 */
        execv("/bin/wc", argv); /* 执行"wc"程序 */
    } else {
        /* 父进程 */
        close(p[0]);                      /* 关闭管道的读端 */
        write(p[1], "hello world\n", 12); /* 向管道写入"hello world\n" */
        close(p[1]);                      /* 关闭管道的写端，表示写入完成 */
    }

    return 0;
}