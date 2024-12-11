#include "../include/newfs.h"

struct nfs_super      nfs_super; 
struct custom_options nfs_options;

/**
 * @brief ��ȡ�ļ���
 * 
 * @param path 
 * @return char* 
 */
char* nfs_get_fname(const char* path) {
    char ch = '/';
    // strrchr��������һ��ָ�룬ָ���ַ��������һ�����ֵ��ַ�ch��λ��(��������)
    char *q = strrchr(path, ch) + 1;
    return q;
}

/**
 * @brief ����·���Ĳ㼶
 * exm: /av/c/d/f
 * -> lvl = 4
 * @param path 
 * @return int 
 */
int nfs_calc_lvl(const char * path) {
    char* str = path;
    int   lvl = 0;
    // ��Ŀ¼
    if (strcmp(path, "/") == 0) {
        return lvl;
    }
    while (*str != NULL) {
        if (*str == '/') {
            lvl++;
        }
        str++;
    }
    return lvl;
}

/**
 * @brief ������
 * 
 * @param offset 
 * @param out_content 
 * @param size 
 * @return int 
 */
int nfs_driver_read(int offset, uint8_t *out_content, int size) {
    // ���룬��һ���߼����С(����IO��С)���ж�д
    int      offset_aligned = NFS_ROUND_DOWN(offset, NFS_BLK_SZ());
    int      bias           = offset - offset_aligned;
    int      size_aligned   = NFS_ROUND_UP((size + bias), NFS_BLK_SZ());
    uint8_t* temp_content   = (uint8_t*)malloc(size_aligned);
    uint8_t* cur            = temp_content;
    
    // ����ͷ��λ��downλ��
    ddriver_seek(NFS_DRIVER(), offset_aligned, SEEK_SET);
    // ����IO��С���ж�����down��ʼ��size_aligned��С������
    while (size_aligned != 0)
    {
        ddriver_read(NFS_DRIVER(), cur, NFS_IO_SZ());
        cur          += NFS_IO_SZ();
        size_aligned -= NFS_IO_SZ();   
    }
    // ��down+bias��ʼ����size��С�����ݵ�out_content
    memcpy(out_content, temp_content + bias, size);
    free(temp_content);
    return NFS_ERROR_NONE;
}

/**
 * @brief ����д
 * 
 * @param offset 
 * @param in_content 
 * @param size 
 * @return int 
 */
int nfs_driver_write(int offset, uint8_t *in_content, int size) {
    // ���룬��һ���߼����С(����IO��С)���ж�д
    int      offset_aligned = NFS_ROUND_DOWN(offset, NFS_BLK_SZ());
    int      bias           = offset - offset_aligned;
    int      size_aligned   = NFS_ROUND_UP((size + bias), NFS_BLK_SZ());
    uint8_t* temp_content   = (uint8_t*)malloc(size_aligned);
    uint8_t* cur            = temp_content;
    // ������д���̿鵽�ڴ�
    nfs_driver_read(offset_aligned, temp_content, size_aligned);
    // ��down+bias��ʼ����size��С������
    memcpy(temp_content + bias, in_content, size);
    // ����ͷ��λ��down
    ddriver_seek(NFS_DRIVER(), offset_aligned, SEEK_SET);

    // �������ڴ����޸ĺ�д�ش���
    while (size_aligned != 0)
    {
        ddriver_write(NFS_DRIVER(), cur, NFS_IO_SZ());
        cur          += NFS_IO_SZ();
        size_aligned -= NFS_IO_SZ();   
    }

    free(temp_content);
    return NFS_ERROR_NONE;
}

/**
 * @brief ��dentry���뵽inode�У�����ͷ�巨
 * 
 * @param inode 
 * @param dentry 
 * @return int 
 */
int nfs_alloc_dentry(struct nfs_inode* inode, struct nfs_dentry* dentry) {
    if (inode->dentrys == NULL) {
        inode->dentrys = dentry;
    }
    else {
        dentry->brother = inode->dentrys;
        inode->dentrys = dentry;
    }
    inode->dir_cnt++;
    inode->size += sizeof(struct nfs_dentry);

    // �ж��Ƿ���Ҫ���·���һ�����ݿ�
    if (inode->dir_cnt % NFS_DENTRY_PER_BLK() == 1) {
        inode->block_pointer[inode->block_allocted] = nfs_alloc_data();
        inode->block_allocted++;
    }
    return inode->dir_cnt;
}

/**
 * @brief ����һ��inode��ռ��λͼ
 * 
 * @param dentry ��dentryָ������inode
 * @return nfs_inode
 */
struct nfs_inode* nfs_alloc_inode(struct nfs_dentry * dentry) {
    struct nfs_inode* inode;
    int byte_cursor   = 0; 
    int bit_cursor    = 0; 
    int ino_cursor    = 0;  
    boolean is_find_free_entry   = FALSE;

    inode = (struct nfs_inode*)malloc(sizeof(struct nfs_inode));

    for (byte_cursor = 0; byte_cursor < NFS_BLKS_SZ(nfs_super.map_inode_blks); byte_cursor++)
    {
        
        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
            if((nfs_super.map_inode[byte_cursor] & (0x1 << bit_cursor)) == 0) {    
                // ��ǰino_cursorλ�ÿ��� 
                nfs_super.map_inode[byte_cursor] |= (0x1 << bit_cursor);
                is_find_free_entry = TRUE;           
                break;
            }
            ino_cursor++;
        }
        if (is_find_free_entry) {
            break;
        }
    }

    if (!is_find_free_entry || ino_cursor >= nfs_super.max_ino)
        return -NFS_ERROR_NOSPACE;

    // ΪĿ¼�����inode�ڵ㲢��ʼ���������
    inode->ino  = ino_cursor; 
    inode->size = 0;
    inode->dir_cnt = 0;
    inode->block_allocted = 0;
    inode->dentrys = NULL;

    // dentryָ������inode 
    dentry->inode = inode;
    dentry->ino   = inode->ino;
    // inodeָ��dentry 
    inode->dentry = dentry;
    
    // �ļ�������Ҫ����ռ�,Ŀ¼���Ѿ���dentrys��,��ͨ�ļ���Ҫ�����������ݿ�Ĳ���,һ���Է�����ͺ���
    if (NFS_IS_REG(inode)) {
        for (int i = 0; i < NFS_DATA_PER_FILE; i++) {
            inode->data[i] = (uint8_t *)malloc(NFS_BLK_SZ());
        }
    }

    return inode;
}

/**
 * @brief �������һ�����ݿ�
 * @return ��������ݿ��
 */
 int nfs_alloc_data() {
    int byte_cursor       = 0; 
    int bit_cursor        = 0;   
    int dno_cursor        = 0; 
    int is_find_free_data = 0;

    for (byte_cursor = 0; byte_cursor < NFS_BLKS_SZ(nfs_super.map_dnode_blks); byte_cursor++) {
        // ���ڸ��ֽ��б���8��bitѰ�ҿ��е�inodeλͼ
        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
            if((nfs_super.map_dnode[byte_cursor] & (0x1 << bit_cursor)) == 0) {
                // ��ǰdno_cursorλ�ÿ���
                nfs_super.map_dnode[byte_cursor] |= (0x1 << bit_cursor);
                is_find_free_data = 1;
                break;
            }
            dno_cursor++;
        }
        if (is_find_free_data) {
            break;
        }
    }

    if (!is_find_free_data || dno_cursor >= nfs_super.max_dno) {
        return -NFS_ERROR_NOSPACE;
    }

    return dno_cursor;
 }

/**
 * @brief ���ڴ�inode�����·��ṹȫ��ˢ�ش���
 * 
 * @param inode 
 * @return int 
 */
int nfs_sync_inode(struct nfs_inode * inode) {
    struct nfs_inode_d  inode_d;
    struct nfs_dentry*  dentry_cursor;
    struct nfs_dentry_d dentry_d;
    int offset;
    int ino             = inode->ino;

    // ��inode�����ݿ�����inode_d��
    inode_d.ino            = ino;
    inode_d.size           = inode->size;
    inode_d.ftype          = inode->dentry->ftype;
    inode_d.dir_cnt        = inode->dir_cnt;
    inode_d.block_allocted = inode->block_allocted;
    for (int i = 0; i < NFS_DATA_PER_FILE; i++) {
        inode_d.block_pointer[i] = inode->block_pointer[i];
    }
    
    // ��inode_dˢ�ش���
    if (nfs_driver_write(NFS_INO_OFS(ino), (uint8_t *)&inode_d, 
                     sizeof(struct nfs_inode_d)) != NFS_ERROR_NONE) {
        NFS_DBG("[%s] io error\n", __func__);
        return -NFS_ERROR_IO;
    }

    // ˢ��inode�����ݿ�
    if (NFS_IS_DIR(inode)) { // �����ǰinode��Ŀ¼����ô������Ŀ¼���Ŀ¼���inodeҲҪд��                         
        dentry_cursor     = inode->dentrys;
        int data_blks_num = 0;
        // Ҫ��dentry������ˢ�ش��̣�Ŀ¼��block_allocted�����ݿ�
        while ((dentry_cursor != NULL) && (data_blks_num < inode->block_allocted)) {
            offset = NFS_DATA_OFS(inode->block_pointer[data_blks_num]);
            while ((dentry_cursor != NULL) && (offset + sizeof(struct nfs_dentry_d) < NFS_DATA_OFS(inode->block_pointer[data_blks_num] + 1))) {
                // dentry�����ݸ��Ƶ�dentry_d��
                memcpy(dentry_d.fname, dentry_cursor->fname, NFS_MAX_FILE_NAME);
                dentry_d.ftype = dentry_cursor->ftype;
                dentry_d.ino = dentry_cursor->ino;
                // dentry_d������ˢ�ش���
                if (nfs_driver_write(offset, (uint8_t *)&dentry_d, sizeof(struct nfs_dentry_d))!= NFS_ERROR_NONE) {
                    NFS_DBG("[%s] io error\n", __func__);
                    return -NFS_ERROR_IO;
                }

                // �ݹ鴦��Ŀ¼�������
                if (dentry_cursor->inode != NULL) {
                    nfs_sync_inode(dentry_cursor->inode);
                }

                dentry_cursor  = dentry_cursor->brother;
                offset += sizeof(struct nfs_dentry_d);
            }
            data_blks_num++;
        }
    }
    else if (NFS_IS_REG(inode)) { 
        // ����ҲҪ��֤i < inode->block_allocted
        // �����ǲ�û��Ϊ�������Ӧ������λͼ���������ﲻ��Ҫд��ȥ
        for (int i = 0; i < inode->block_allocted; i++) {
            if (nfs_driver_write(NFS_DATA_OFS(inode->block_pointer[i]), inode->data[i], NFS_BLKS_SZ(NFS_DATA_PER_FILE)) != NFS_ERROR_NONE) {
                NFS_DBG("[%s] io error\n", __func__);
                return -NFS_ERROR_IO;
            }
        }
    }

    return NFS_ERROR_NONE;
}

/**
 * @brief 
 * 
 * @param dentry dentryָ��ino����ȡ��inode
 * @param ino inodeΨһ���
 * @return struct nfs_inode* 
 */
struct nfs_inode* nfs_read_inode(struct nfs_dentry * dentry, int ino) {
    struct nfs_inode* inode = (struct nfs_inode*)malloc(sizeof(struct nfs_inode));
    struct nfs_inode_d inode_d;
    struct nfs_dentry* sub_dentry;
    struct nfs_dentry_d dentry_d;
    int    dir_cnt = 0;

    // �Ӵ��̶�������� 
    if (nfs_driver_read(NFS_INO_OFS(ino), (uint8_t *)&inode_d, 
                        sizeof(struct nfs_inode_d)) != NFS_ERROR_NONE) {
        NFS_DBG("[%s] io error\n", __func__);
        return NULL;                    
    }
    // ����inode_d�����ݳ�ʼ��inode
    inode->dir_cnt = 0;
    inode->ino = inode_d.ino;
    inode->size = inode_d.size;
    inode->dentry = dentry;
    inode->dentrys = NULL;
    inode->block_allocted = inode_d.block_allocted;
    for (int i = 0; i < NFS_DATA_PER_FILE; i++) {
        inode->block_pointer[i] = inode_d.block_pointer[i];
    }

    // �ڴ��е�inode�����ݻ���Ŀ¼���Ҳ��Ҫ���� 
    if (NFS_IS_DIR(inode)) {
        dir_cnt           = inode_d.dir_cnt;
        int data_blks_num = 0;
        int offset;

        // �����е�Ŀ¼����д���(�ȷ����ݿ鴦��)
        while((dir_cnt > 0) && (data_blks_num < NFS_DATA_PER_FILE)){
            offset = NFS_DATA_OFS(inode->block_pointer[data_blks_num]);

            // �ٷֵ�����Ŀ¼����д���
            while((dir_cnt > 0) && (offset + sizeof(struct nfs_dentry_d) < NFS_DATA_OFS(inode->block_pointer[data_blks_num] + 1))){
                if (nfs_driver_read(offset, (uint8_t *)&dentry_d, sizeof(struct nfs_dentry_d)) != NFS_ERROR_NONE){
                    NFS_DBG("[%s] io error\n", __func__);
                    return NULL;  
                }
                
                // �ôӴ����ж�����dentry_d�����ڴ��е�sub_dentry 
                sub_dentry = new_dentry(dentry_d.fname, dentry_d.ftype);
                sub_dentry->parent = inode->dentry;
                sub_dentry->ino    = dentry_d.ino; 
                nfs_alloc_dentry(inode, sub_dentry);

                offset += sizeof(struct nfs_dentry_d);
                dir_cnt--;
            }
            data_blks_num++;
        }
    }
    // inode����ͨ�ļ���ֱ�Ӷ�ȡ���ݿ�
    else if (NFS_IS_REG(inode)) {
        for (int i = 0; i < NFS_DATA_PER_FILE; i++) {
            inode->data[i] = (uint8_t *)malloc(NFS_BLKS_SZ(NFS_DATA_PER_FILE));
            if (nfs_driver_read(NFS_DATA_OFS(inode->block_pointer[i]), (uint8_t *)inode->data[i], 
                            NFS_BLKS_SZ(NFS_DATA_PER_FILE)) != NFS_ERROR_NONE) {
                NFS_DBG("[%s] io error\n", __func__);
                return NULL;                    
            }
        }
    }

    return inode;
}

/**
 * @brief 
 * 
 * @param inode 
 * @param dir [0...]
 * @return struct nfs_dentry* 
 */
struct nfs_dentry* nfs_get_dentry(struct nfs_inode * inode, int dir) {
    struct nfs_dentry* dentry_cursor = inode->dentrys;
    int    cnt = 0;
    // ��ȡĳ��inode�ĵ�dir��Ŀ¼��
    while (dentry_cursor)
    {
        if (dir == cnt) {
            return dentry_cursor;
        }
        cnt++;
        dentry_cursor = dentry_cursor->brother;
    }
    return NULL;
}

/**
 * @brief �����ļ���Ŀ¼
 * path: /qwe/ad  total_lvl = 2,
 *      1) find /'s inode       lvl = 1
 *      2) find qwe's dentry 
 *      3) find qwe's inode     lvl = 2
 *      4) find ad's dentry
 *
 * path: /qwe     total_lvl = 1,
 *      1) find /'s inode       lvl = 1
 *      2) find qwe's dentry
 *  
 * 
 * ����ܲ��ҵ������ظ�Ŀ¼��
 * ������Ҳ��������ص�����һ����Ч��·��
 * 
 * path: /a/b/c
 *      1) find /'s inode     lvl = 1
 *      2) find a's dentry 
 *      3) find a's inode     lvl = 2
 *      4) find b's dentry    �����ʱ�Ҳ����ˣ�is_find=FALSE�ҷ��ص���a��inode��Ӧ��dentry
 * 
 * @param path 
 * @return struct nfs_dentry* 
 */
struct nfs_dentry* nfs_lookup(const char * path, boolean* is_find, boolean* is_root) {
    struct nfs_dentry* dentry_cursor = nfs_super.root_dentry;
    struct nfs_dentry* dentry_ret = NULL;
    struct nfs_inode*  inode; 
    int   total_lvl = nfs_calc_lvl(path);
    int   lvl       = 0;
    boolean is_hit;
    char* fname     = NULL;
    char* path_cpy  = (char*)malloc(sizeof(path));
    *is_root        = FALSE;
    strcpy(path_cpy, path);

    // ��Ŀ¼ 
    if (total_lvl == 0) {                           
        *is_find = TRUE;
        *is_root = TRUE;
        dentry_ret = nfs_super.root_dentry;
    }

    // strtok�����ָ�·���Դ˻��������ļ���
    fname = strtok(path_cpy, "/");       
    while (fname)
    {   
        lvl++;
        // Cache����,�����ǰdentry��inodeΪ����Ӵ��̶�����
        if (dentry_cursor->inode == NULL) {           
            nfs_read_inode(dentry_cursor, dentry_cursor->ino);
        }

        inode = dentry_cursor->inode;

        // ��û����Ӧ�����Ͳ鵽��ͨ�ļ����޷��������²�ѯ
        if (NFS_IS_REG(inode) && lvl < total_lvl) {
            NFS_DBG("[%s] not a dir\n", __func__);
            dentry_ret = inode->dentry;
            break;
        }

        // ��ǰinode��Ӧ����һ��Ŀ¼
        if (NFS_IS_DIR(inode)) {
            dentry_cursor = inode->dentrys;
            is_hit        = FALSE;

            // ������Ŀ¼���ҵ���Ӧ�ļ����Ƶ�dentry
            while (dentry_cursor)   
            {
                if (memcmp(dentry_cursor->fname, fname, strlen(fname)) == 0) {
                    is_hit = TRUE;
                    break;
                }
                dentry_cursor = dentry_cursor->brother;
            }
            
            // û���ҵ���Ӧ�ļ������Ƶ�Ŀ¼���򷵻�����ҵ����ļ��е�dentry
            if (!is_hit) {
                *is_find = FALSE;
                NFS_DBG("[%s] not found %s\n", __func__, fname);
                dentry_ret = inode->dentry;
                break;
            }

            if (is_hit && lvl == total_lvl) {
                *is_find = TRUE;
                dentry_ret = dentry_cursor;
                break;
            }
        }
        // ����ʹ��strtok��ȡ��һ���ļ�������
        fname = strtok(NULL, "/"); 
    }

    // �ٳ�ȷ��dentry_cursor��Ӧ��inode��Ϊ��
    if (dentry_ret->inode == NULL) {
        dentry_ret->inode = nfs_read_inode(dentry_ret, dentry_ret->ino);
    }
    
    return dentry_ret;
}

/**
 * @brief ����nfs, Layout ����
 * 
 * Layout
 * ÿ8��Inodeռ��һ��Blk
 * @param options 
 * @return int 
 */
int nfs_mount(struct custom_options options){
    int                 ret = NFS_ERROR_NONE;
    int                 driver_fd;
    struct nfs_super_d  nfs_super_d; 
    struct nfs_dentry*  root_dentry;
    struct nfs_inode*   root_inode;

    int                 inode_num;
    int                 map_inode_blks;
    int                 data_num;
    int                 map_dnode_blks;
    
    int                 super_blks;
    boolean             is_init = FALSE;

    nfs_super.is_mounted = FALSE;
    driver_fd = ddriver_open(options.device);

    if (driver_fd < 0) {
        return driver_fd;
    }

    // �򳬼�����д�������Ϣ
    nfs_super.fd = driver_fd;
    ddriver_ioctl(NFS_DRIVER(), IOC_REQ_DEVICE_SIZE,  &nfs_super.sz_disk);
    ddriver_ioctl(NFS_DRIVER(), IOC_REQ_DEVICE_IO_SZ, &nfs_super.sz_io);
    nfs_super.sz_blks = 2 * nfs_super.sz_io;  // 1024B


    // ��ȡ���̳���������
    if (nfs_driver_read(NFS_SUPER_OFS, (uint8_t *)(&nfs_super_d), sizeof(struct nfs_super_d)) != NFS_ERROR_NONE) {
        return -NFS_ERROR_IO;
    }   
    
    // ���������Ⱦ��ǵ�һ�ι���
    if (nfs_super_d.magic != NFS_MAGIC_NUM) {    

        super_blks      = NFS_SUPER_BLKS;
        map_inode_blks  = NFS_INODE_MAP_BLKS;
        map_dnode_blks   = NFS_DATA_MAP_BLKS;
        inode_num       = NFS_INODE_BLKS;
        data_num        = NFS_DATA_BLKS;

        // ����layout 
        nfs_super.max_ino               = inode_num;
        nfs_super.max_dno               = data_num;
        nfs_super_d.map_inode_blks      = map_inode_blks; 
        nfs_super_d.map_dnode_blks       = map_dnode_blks; 
        nfs_super_d.map_inode_offset    = NFS_SUPER_OFS + NFS_BLKS_SZ(super_blks);
        nfs_super_d.map_dnode_offset     = nfs_super_d.map_inode_offset + NFS_BLKS_SZ(map_inode_blks);
        nfs_super_d.inode_offset        = nfs_super_d.map_dnode_offset + NFS_BLKS_SZ(map_dnode_blks);
        nfs_super_d.data_offset         = nfs_super_d.inode_offset + NFS_BLKS_SZ(inode_num);

        nfs_super_d.sz_usage            = 0;
        nfs_super_d.magic               = NFS_MAGIC_NUM;

        is_init = TRUE;
    }

    // ���� in-memory �ṹ 
    // ��ʼ��������
    nfs_super.sz_usage   = nfs_super_d.sz_usage; 

    // ��������λͼ    
    nfs_super.map_inode         = (uint8_t *)malloc(NFS_BLKS_SZ(nfs_super_d.map_inode_blks));
    nfs_super.map_inode_blks    = nfs_super_d.map_inode_blks;
    nfs_super.map_inode_offset  = nfs_super_d.map_inode_offset;
    nfs_super.inode_offset      = nfs_super_d.inode_offset;

    // ��������λͼ
    nfs_super.map_dnode          = (uint8_t *)malloc(NFS_BLKS_SZ(nfs_super_d.map_dnode_blks));
    nfs_super.map_dnode_blks     = nfs_super_d.map_dnode_blks;
    nfs_super.map_dnode_offset   = nfs_super_d.map_dnode_offset;
    nfs_super.data_offset       = nfs_super_d.data_offset;

    // �Ӵ����ж�ȡ����λͼ
    if (nfs_driver_read(nfs_super_d.map_inode_offset, (uint8_t *)(nfs_super.map_inode), 
                        NFS_BLKS_SZ(nfs_super_d.map_inode_blks)) != NFS_ERROR_NONE) {
        return -NFS_ERROR_IO;
    }

    // �Ӵ����ж�ȡ����λͼ
    if (nfs_driver_read(nfs_super_d.map_dnode_offset, (uint8_t *)(nfs_super.map_dnode), 
                        NFS_BLKS_SZ(nfs_super_d.map_dnode_blks)) != NFS_ERROR_NONE) {
        return -NFS_ERROR_IO;
    }
    // �½���Ŀ¼
    root_dentry = new_dentry("/", NFS_DIR);
    // ������ڵ� 
    if (is_init) {                                    
        root_inode = nfs_alloc_inode(root_dentry);
        nfs_sync_inode(root_inode);
    }
    
    root_inode            = nfs_read_inode(root_dentry, NFS_ROOT_INO);
    root_dentry->inode    = root_inode;
    nfs_super.root_dentry = root_dentry;
    nfs_super.is_mounted  = TRUE;

    return ret;
}

/**
 * @brief ж��nfs
 * 
 * @return int 
 */
int nfs_umount() {
    struct nfs_super_d  nfs_super_d; 

    // û�й���ֱ�ӱ���
    if (!nfs_super.is_mounted) {
        return NFS_ERROR_NONE;
    }

    // �Ӹ��ڵ�����ˢд�ڵ� 
    nfs_sync_inode(nfs_super.root_dentry->inode);     

    // ���ڴ��еĳ�����ˢ�ش���                              
    nfs_super_d.magic               = NFS_MAGIC_NUM;
    nfs_super_d.sz_usage            = nfs_super.sz_usage;

    nfs_super_d.map_inode_blks      = nfs_super.map_inode_blks;
    nfs_super_d.map_inode_offset    = nfs_super.map_inode_offset;
    nfs_super_d.inode_offset        = nfs_super.inode_offset;

    nfs_super_d.map_dnode_blks       = nfs_super.map_dnode_blks;
    nfs_super_d.map_dnode_offset     = nfs_super.map_dnode_offset;
    nfs_super_d.data_offset         = nfs_super.data_offset;
    
    if (nfs_driver_write(NFS_SUPER_OFS, (uint8_t *)&nfs_super_d, 
                     sizeof(struct nfs_super_d)) != NFS_ERROR_NONE) {
        return -NFS_ERROR_IO;
    }

    // ������λͼˢ�ش��� 
    if (nfs_driver_write(nfs_super_d.map_inode_offset, (uint8_t *)(nfs_super.map_inode), 
                         NFS_BLKS_SZ(nfs_super_d.map_inode_blks)) != NFS_ERROR_NONE) {
        return -NFS_ERROR_IO;
    }

    // ������λͼˢ�ش���
    if (nfs_driver_write(nfs_super_d.map_dnode_offset, (uint8_t *)(nfs_super.map_dnode), 
                         NFS_BLKS_SZ(nfs_super_d.map_dnode_blks)) != NFS_ERROR_NONE) {
        return -NFS_ERROR_IO;
    }

    // �ͷ��ڴ��е�λͼ
    free(nfs_super.map_inode);
    free(nfs_super.map_dnode);

    // �ر����� 
    ddriver_close(NFS_DRIVER());

    return NFS_ERROR_NONE;
}