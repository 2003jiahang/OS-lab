#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char *fmtname(char *path);

char *fmtname(char *path) {
  static char buf[DIRSIZ + 1];
  char *p;

  // Find first character after last slash.
  for (p = path + strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if (strlen(p) >= DIRSIZ) return p;
  memmove(buf, p, strlen(p));
  buf[strlen(p)] = 0;
  return buf;
}

void find(char *path, char *target) {
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if ((fd = open(path, 0)) < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

    switch (st.type) {
    case T_FILE:
        if (strcmp(fmtname(path), target) == 0) {
        printf("%s\n", path);
        }
        break;

    case T_DIR:
        // 确保当前目录名符合目标，并且在递归之前输出
        if (strcmp(fmtname(path), target) == 0) {
        printf("%s\n", path);
        }

        // 确保路径大小合适
        if (strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)) {
        printf("find: path too long\n");
        break;
        }

        // 复制当前路径并加上斜杠
        strcpy(buf, path);
        p = buf + strlen(buf);
        *p++ = '/';

        // 遍历目录下的所有文件和子目录
        while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0) continue;
        // 跳过当前目录和父目录
        if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) continue;

        // 复制目录项的名称到路径缓冲区
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;

        // 递归调用 find 函数继续处理
        if (stat(buf, &st) < 0) {
            printf("find: cannot stat %s\n", buf);
            continue;
        }

        find(buf, target);  // 递归查找
        }
        break;
    }

  close(fd);
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(2, "Usage: find <directory> <filename>\n");
    exit(1);
  }
  find(argv[1], argv[2]);
  exit(0);
}
