# file system

## large files


block  布局 (boot, super, log, inode , bitmap) block

struct dinode 描述inode在磁盘上的数据 相关参数 NDIRECT MAXFILE等

读写file时 会call bmap， 分配新的block
    bn : logical block number 文件的相对地址
    ip->addr bread() 参数时实际disk block number
    mapping a file's logical block numbers into disk block numbers.

前11 direct 12 single 13 doubly

hints
    理解bmap  画个表
    如何在doubly-indirect 索引
    inode dinode同步修改
    bread 之后 brelse
    allocate as needed
    itrunc free 所有block


实现:
    极困下bug频出

    修改NDIRECT， MASFILE(!!!!) 等参数
    NDIRECT 一级indirect， NDIRECT + 1， 二级indirect
    二级查找写的较垃圾，大量的if else以及重复代码, 把我自己搞晕了

Todo:
    确认逻辑意义，file system底层原理
    log_write干什么的


# Symbolic links
    增加符号连接 / 软连接 refer to 一个linked file by pathname

    扩展symlink

    新建system call, 修改usys.pl ，user.h ，扩展sysfile.c
    kernel/stat.h 增加新的file typle T_SYMLINK
    fcntl.h增加flag(O_NOFOLLOW)，配和 open OR传入，不要覆盖
    在inode的data存储target 返回 0 / -1
    修改open，应对symbolic情况 打开symlink的文件而不是link
    考虑循环 / 圈情况。 增加最大深度限制
    只应修改open适配 symbolic，其他的应该不变 ？(write?)
    lab 不用处理链接到目录的情况







