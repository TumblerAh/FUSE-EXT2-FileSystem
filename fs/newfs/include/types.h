#ifndef _TYPES_H_
#define _TYPES_H_

/******************************************************************************
* SECTION: Type def
*******************************************************************************/
typedef int          boolean;
typedef uint16_t     flag16;

typedef enum nfs_file_type {
    NFS_REG_FILE, //��ͨ�ļ�
    NFS_DIR, //Ŀ¼�ļ�
    
} NFS_FILE_TYPE;

/******************************************************************************
* SECTION: Macro
*******************************************************************************/
#define TRUE                    1
#define FALSE                   0
#define UINT32_BITS             32
#define UINT8_BITS              8

#define NFS_MAGIC_NUM           0x52415453  
#define NFS_SUPER_OFS           0
#define NFS_ROOT_INO            0

#define NFS_ERROR_NONE          0
#define NFS_ERROR_ACCESS        EACCES
#define NFS_ERROR_SEEK          ESPIPE     
#define NFS_ERROR_ISDIR         EISDIR
#define NFS_ERROR_NOSPACE       ENOSPC
#define NFS_ERROR_EXISTS        EEXIST
#define NFS_ERROR_NOTFOUND      ENOENT
#define NFS_ERROR_UNSUPPORTED   ENXIO
#define NFS_ERROR_IO            EIO     /* Error Input/Output */
#define NFS_ERROR_INVAL         EINVAL  /* Invalid Args */
#define NFS_FLAG_BUF_DIRTY      0x1
#define NFS_FLAG_BUF_OCCUPY     0x2
#define NFS_IOC_MAGIC           'S'
#define NFS_IOC_SEEK            _IO(NFS_IOC_MAGIC, 0)

#define NFS_MAX_FILE_NAME       128
#define NFS_INODE_PER_FILE      1 //һ���ļ���Ӧһ��inode
#define NFS_DATA_PER_FILE       15//����ֱ�������ķ�ʽ��һ���ļ����ʹ��15�����ݿ�
#define NFS_DEFAULT_PERM        0777





// ���̲������,һ���߼����ܷ�16��inode
/**
 * ���������趨һ���߼������������Դ洢16��inode��Ҳ����һ��inode��Ҫ64B��С
 * һ���ļ����ʹ��16�����ݿ飬��ô4MB�����Դ洢4MB/(16KB+64B)=255���ļ�
 * 255���ļ���Ҫ255��������һ��������Ӧ64B��С�����������ڵ��������Ҫ16���߼���Ϳ��Դ洢ȫ�������ڵ�
 * ��ôһ�����ڳ�������������λͼ��16�������ڵ㣬��ô���ݿ�����4096-1-1-1-16=4077���߼���
 */
#define NFS_INODE_PER_BLK       16     // һ���߼����ܷ�16��inode
#define NFS_SUPER_BLKS          1 //������
#define NFS_INODE_MAP_BLKS      1//�����ڵ�λͼ
#define NFS_DATA_MAP_BLKS       1 //����λͼ
#define NFS_INODE_BLKS          16    // 4096/16/16(���̴�СΪ4096���߼��飬ά��һ���ļ���Ҫ16���߼��鼴һ��������+15�����ݿ�)
#define NFS_DATA_BLKS           4077  // 4096-32-1-1-1

/******************************************************************************
* SECTION: Macro Function
*******************************************************************************/
#define NFS_IO_SZ()                     (nfs_super.sz_io)    // 512B
#define NFS_BLK_SZ()                    (nfs_super.sz_blks)  // 1KB
#define NFS_DISK_SZ()                   (nfs_super.sz_disk)  // 4MB
#define NFS_DRIVER()                    (nfs_super.fd)
#define NFS_BLKS_SZ(blks)               ((blks) * NFS_BLK_SZ())
#define NFS_DENTRY_PER_BLK()            (NFS_BLK_SZ() / sizeof(struct nfs_dentry))

// ����ȡ���Լ�����ȡ��
#define NFS_ROUND_DOWN(value, round)    ((value) % (round) == 0 ? (value) : ((value) / (round)) * (round))
#define NFS_ROUND_UP(value, round)      ((value) % (round) == 0 ? (value) : ((value) / (round) + 1) * (round))

// �����ļ�����ĳ��dentry��
#define NFS_ASSIGN_FNAME(pnfs_dentry, _fname) memcpy(pnfs_dentry->fname, _fname, strlen(_fname))

// ����inode��data��ƫ����                                     
#define NFS_INO_OFS(ino)                (nfs_super.inode_offset + NFS_BLKS_SZ(ino))   // inode����ַ��ʼƫ��+ǰ���inodeռ�õĿռ�
#define NFS_DATA_OFS(dno)               (nfs_super.data_offset + NFS_BLKS_SZ(dno))     // data����ַ��ʼƫ��+ǰ���dataռ�õĿռ�

// �ж�inodeָ�������Ŀ¼������ͨ�ļ�
#define NFS_IS_DIR(pinode)              (pinode->dentry->ftype == NFS_DIR)
#define NFS_IS_REG(pinode)              (pinode->dentry->ftype == NFS_REG_FILE)

/******************************************************************************
* SECTION: FS Specific Structure - In memory structure
*******************************************************************************/
struct custom_options {
	const char*        device;//������·��
};

struct nfs_super {
    uint32_t           magic;             // ����
    int                fd;                // �ļ�������

    int                sz_io;             // 512B
    int                sz_blks;           // 1KB
    int                sz_disk;           // 4MB
    int                sz_usage;          // ��ʹ�ÿռ��С

    int                max_ino;           // �����ڵ��������
    uint8_t*           map_inode;         // inodeλͼ�ڴ����
    int                map_inode_blks;    // inodeλͼռ�õ��߼�������
    int                map_inode_offset;  // inodeλͼ�ڴ����е�ƫ��

    int                max_dno;           // ����λͼ�������
    uint8_t*           map_dnode;          // dataλͼ�ڴ����
    int                map_dnode_blks;     // dataλͼռ�õ��߼�������
    int                map_dnode_offset;   // dataλͼ�ڴ����е�ƫ��

    int                inode_offset;      // inode�ڴ����е�ƫ��
    int                data_offset;       // data�ڴ����е�ƫ��

    boolean            is_mounted;        // �Ƿ����
    struct nfs_dentry* root_dentry;       // ��Ŀ¼dentry
};

struct nfs_inode {
    /* TODO: Define yourself */
    uint32_t           ino;                               // �������                         
    int                size;                              // �ļ�ռ�ÿռ�
    int                link;                              // ������Ĭ��Ϊ1(�����������Ӻ�Ӳ����)
    int                block_pointer[NFS_DATA_PER_FILE];  // ���ݿ�����
    int                dir_cnt;                           // �����Ŀ¼���ļ���������м���Ŀ¼��
    struct nfs_dentry  *dentry;                           // ָ���inode�ĸ�dentry
    struct nfs_dentry  *dentrys;                          // ָ���inode��������dentry
    NFS_FILE_TYPE      ftype;                             // �ļ�����
    uint8_t*           data[NFS_DATA_PER_FILE];           // ָ�����ݿ��ָ��
    int                block_allocted;                    // �ѷ������ݿ�����
};

struct nfs_dentry {
    /* TODO: Define yourself */
    char               fname[NFS_MAX_FILE_NAME];    // dentryָ����ļ���
    struct nfs_dentry* parent;                      // ��Ŀ¼��dentry            
    struct nfs_dentry* brother;                     // �ֵ�dentry
    int                ino;                         // ָ���inode���
    struct nfs_inode*  inode;                       // ָ���inode  
    NFS_FILE_TYPE      ftype;                       // �ļ�����

};

// �����µ�dentry
static inline struct nfs_dentry* new_dentry(char * fname, NFS_FILE_TYPE ftype) {
    struct nfs_dentry * dentry = (struct nfs_dentry *)malloc(sizeof(struct nfs_dentry));
    memset(dentry, 0, sizeof(struct nfs_dentry));
    NFS_ASSIGN_FNAME(dentry, fname);
    dentry->ftype   = ftype;
    dentry->ino     = -1;
    dentry->inode   = NULL;
    dentry->parent  = NULL;
    dentry->brother = NULL;  
    return dentry;                                          
}

/******************************************************************************
* SECTION: FS Specific Structure - To Disk structure
*******************************************************************************/
struct nfs_super_d {
    uint32_t           magic;             // ����
    int                sz_usage;          // ��ʹ�ÿռ��С

    int                max_ino;           // �����ڵ��������
    int                map_inode_blks;    // inodeλͼռ�õ��߼�������
    int                map_inode_offset;  // inodeλͼ�ڴ����е�ƫ��

    int                max_dno;           // ����λͼ�������
    int                map_dnode_blks;     // dataλͼռ�õ��߼�������
    int                map_dnode_offset;   // dataλͼ�ڴ����е�ƫ��

    int                inode_offset;      // inode�ڴ����е�ƫ��
    int                data_offset;       // data�ڴ����е�ƫ��
};

struct nfs_inode_d {
    uint32_t           ino;                               // �������                         
    int                size;                              // �ļ�ռ�ÿռ�(���˶��ٸ��߼���) 
    int                link;                              // ������Ĭ��Ϊ1(�����������Ӻ�Ӳ����)
    int                block_pointer[NFS_DATA_PER_FILE];  // ���ݿ�����
    int                dir_cnt;                           // �����Ŀ¼���ļ���������м���Ŀ¼��
    NFS_FILE_TYPE      ftype;                             // �ļ�����
    int                block_allocted;                    // �ѷ������ݿ�����
};

struct nfs_dentry_d {
    char               fname[NFS_MAX_FILE_NAME];    // dentryָ����ļ���
    int                ino;                         // ָ���inode���
    NFS_FILE_TYPE      ftype;                       // �ļ�����
};

#endif /* _TYPES_H_ */