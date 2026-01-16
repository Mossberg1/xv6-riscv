#include <types.h>
#include <riscv.h>
#include <defs.h>
#include <param.h>
#include <memlayout.h>
#include <spinlock.h>
#include <sleeplock.h>
#include <virtio.h>

#define BACKGROUND_COLOR 0x00336699

#define R(r) ((volatile uint32 *)(VIRTIO1 + (r)))

// 2d commands
#define VIRTIO_GPU_CMD_GET_DISPLAY_INFO        0x0100
#define VIRTIO_GPU_CMD_RESOURCE_CREATE_2D      0x0101
#define VIRTIO_GPU_CMD_RESOURCE_UNREF          0x0102
#define VIRTIO_GPU_CMD_SET_SCANOUT             0x0103
#define VIRTIO_GPU_CMD_RESOURCE_FLUSH          0x0104
#define VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D     0x0105
#define VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING 0x0106
#define VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING 0x0107
#define VIRTIO_GPU_CMD_GET_CAPSET_INFO         0x0108
#define VIRTIO_GPU_CMD_GET_CAPSET              0x0109
#define VIRTIO_GPU_CMD_GET_EDID                0x0110

// Cursor commands
#define VIRTIO_GPU_CMD_UPDATE_CURSOR           0x0300
#define VIRTIO_GPU_CMD_MOVE_CURSOR             0x0301

// Success responses
#define VIRTIO_GPU_RESP_OK_NODATA       0x1100
#define VIRTIO_GPU_RESP_OK_DISPLAY_INFO 0x1101
#define VIRTIO_GPU_RESP_OK_CAPSET_INFO  0x1102
#define VIRTIO_GPU_RESP_OK_CAPSET       0x1103
#define VIRTIO_GPU_RESP_OK_EDID         0x1104

// Error responses
#define VIRTIO_GPU_RESP_ERR_UNSPEC              0x1200
#define VIRTIO_GPU_RESP_ERR_OUT_OF_MEMORY       0x1201
#define VIRTIO_GPU_RESP_ERR_INVALID_SCANOUT_ID  0x1202
#define VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID 0x1203
#define VIRTIO_GPU_RESP_ERR_INVALID_CONTEXT_ID  0x1204
#define VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER   0x1205

#define VIRTIO_GPU_FLAG_FENCE (1 << 0)

// Pixel formats
#define VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM 1
#define VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM 2
#define VIRTIO_GPU_FORMAT_A8R8G8B8_UNORM 3
#define VIRTIO_GPU_FORMAT_X8R8G8B8_UNORM 4
#define VIRTIO_GPU_FORMAT_R8G8B8A8_UNORM 67
#define VIRTIO_GPU_FORMAT_X8B8G8R8_UNORM 68
#define VIRTIO_GPU_FORMAT_A8B8G8R8_UNORM 121
#define VIRTIO_GPU_FORMAT_R8G8B8X8_UNORM 134

// Target 640x480 = 1,228,800 bytes = 300 pages
#define FB_WIDTH  640
#define FB_HEIGHT 400
#define FB_BPP    4
#define FB_SIZE   (FB_WIDTH * FB_HEIGHT * FB_BPP)
#define FB_PAGES  ((FB_SIZE + PGSIZE - 1) / PGSIZE)

// Virtqueue descriptor flags
#define VIRTQ_DESC_F_NEXT  1
#define VIRTQ_DESC_F_WRITE 2

#define RESOURCE_ID 1
#define NUM 8  // virtqueue size

struct virtio_gpu_ctrl_hdr {
  uint32 type;
  uint32 flags;
  uint64 fence_id;
  uint32 ctx_id;
  uint32 padding;
}__attribute__((packed));

struct virtio_gpu_rect {
  uint32 x, y, width, height;
}__attribute__((packed));

struct virtio_gpu_display_one {
  struct virtio_gpu_rect r;
  uint32 enabled;
  uint32 flags;
}__attribute__((packed));

struct virtio_gpu_resp_display_info {
  struct virtio_gpu_ctrl_hdr hdr;
  struct virtio_gpu_display_one pmodes[16];
}__attribute__((packed));

struct virtio_gpu_resource_create_2d {
  struct virtio_gpu_ctrl_hdr hdr;
  uint32 resource_id;
  uint32 format;
  uint32 width;
  uint32 height;
}__attribute__((packed));

struct virtio_gpu_resource_unref { 
  struct virtio_gpu_ctrl_hdr hdr; 
  uint32 resource_id; 
  uint32 padding; 
}__attribute__((packed)); 

struct virtio_gpu_resource_attach_backing {
  struct virtio_gpu_ctrl_hdr hdr;
  uint32 resource_id;
  uint32 nr_entries;
}__attribute__((packed));

struct virtio_gpu_resource_detach_backing { 
  struct virtio_gpu_ctrl_hdr hdr; 
  uint32 resource_id; 
  uint32 padding; 
}__attribute__((packed)); 

struct virtio_gpu_mem_entry {
  uint64 addr;
  uint32 length;
  uint32 padding;
}__attribute__((packed));

struct virtio_gpu_set_scanout {
  struct virtio_gpu_ctrl_hdr hdr;
  struct virtio_gpu_rect r;
  uint32 scanout_id;
  uint32 resource_id;
}__attribute__((packed));

struct virtio_gpu_transfer_to_host_2d {
  struct virtio_gpu_ctrl_hdr hdr;
  struct virtio_gpu_rect r;
  uint64 offset;
  uint32 resource_id;
  uint32 padding;
}__attribute__((packed));

struct virtio_gpu_resource_flush {
  struct virtio_gpu_ctrl_hdr hdr;
  struct virtio_gpu_rect r;
  uint32 resource_id;
  uint32 padding;
}__attribute__((packed));

struct virtio_gpu_cursor_pos { 
  uint32 scanout_id; 
  uint32 x; 
  uint32 y; 
  uint32 padding; 
}__attribute__((packed)); 
 
struct virtio_gpu_update_cursor { 
  struct virtio_gpu_ctrl_hdr hdr; 
  struct virtio_gpu_cursor_pos pos; 
  uint32 resource_id; 
  uint32 hot_x; 
  uint32 hot_y; 
  uint32 padding; 
}__attribute__((packed)); 

static struct {
  struct virtq_desc *desc;
  struct virtq_avail *avail;
  struct virtq_used *used;
  
  char free[NUM];
  uint16 used_idx;
  
  struct spinlock lock;

  // Since this is a global static variable, VAddr == PAddr.
  // The GPU can safely read this from anywhere.
  char cmd_buf[PGSIZE];
  char resp_buf[PGSIZE]; 
} gpu ;

static void *fb_pages[FB_PAGES];
//static uint32 *framebuffer;
static uint32 fb_width, fb_height;

static int
alloc_desc(void)
{
  for (int i = 0; i < NUM; i++) {
    if (gpu.free[i]) {
      gpu.free[i] = 0;
      return i;
    }
  }
  return -1;
}

static void
free_desc(int i)
{
  gpu.free[i] = 1;
}

static void
gpu_send_cmd(void *cmd, int cmd_size, void *resp, int resp_size)
{
  acquire(&gpu.lock);

  if (cmd_size > PGSIZE) panic("virtio_gpu: cmd too big");

  // 1. COPY command from Stack (Virtual) to Global Buffer (Physical)
  memmove(gpu.cmd_buf, cmd, cmd_size);
  
  if (resp && resp_size > 0) memset(gpu.resp_buf, 0, resp_size);
  
  int idx[2];
  idx[0] = alloc_desc();
  idx[1] = alloc_desc();

  printf("gpu_send_cmd: desc %d, %d\n", idx[0], idx[1]);
  
  // Command descriptor (device reads)
  gpu.desc[idx[0]].addr = (uint64)gpu.cmd_buf;
  gpu.desc[idx[0]].len = cmd_size;
  gpu.desc[idx[0]].flags = VIRTQ_DESC_F_NEXT;
  gpu.desc[idx[0]].next = idx[1];
  
  // Response descriptor (device writes)
  gpu.desc[idx[1]].addr = (uint64)gpu.resp_buf;
  gpu.desc[idx[1]].len = resp_size;
  gpu.desc[idx[1]].flags = VIRTQ_DESC_F_WRITE;
  gpu.desc[idx[1]].next = 0;
  
  gpu.avail->ring[gpu.avail->idx % NUM] = idx[0];
  __sync_synchronize();
  gpu.avail->idx++;
  __sync_synchronize();
  
  printf("gpu_send_cmd: notifying queue, avail->idx=%d\n", gpu.avail->idx);
  *R(VIRTIO_MMIO_QUEUE_NOTIFY) = 0;

  printf("gpu_send_cmd: waiting for response (used_idx=%d, used->idx=%d)\n", 
        gpu.used_idx, gpu.used->idx);
  
  // Wait for completion
  /*
  while (gpu.used_idx == gpu.used->idx)
    ;
  */

  // Wait for completion with timeout for debugging DEBUG
  int timeout = 100000000;
  while (gpu.used_idx == gpu.used->idx) {
    if (--timeout == 0) {
      printf("gpu_send_cmd: TIMEOUT! used_idx=%d, used->idx=%d\n", 
             gpu.used_idx, gpu.used->idx);
      break;
    }
  }
  // DEBUG

  printf("gpu_send_cmd: done, used->idx=%d\n", gpu.used->idx);
  gpu.used_idx++;

  // 3. Copy response back to stack
  if (resp && resp_size > 0) {
    memmove(resp, gpu.resp_buf, resp_size);
  }
  
  free_desc(idx[0]);
  free_desc(idx[1]);
  
  release(&gpu.lock);
}

void
gpu_flush(void)
{
  // Transfer to host
  struct virtio_gpu_transfer_to_host_2d cmd_transfer = {
    .hdr.type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D,
    .r = { .x = 0, .y = 0, .width = fb_width, .height = fb_height },
    .offset = 0,
    .resource_id = RESOURCE_ID,
  };
  struct virtio_gpu_ctrl_hdr resp_transfer;
  gpu_send_cmd(&cmd_transfer, sizeof(cmd_transfer), &resp_transfer, sizeof(resp_transfer));
  
  // Flush to display
  struct virtio_gpu_resource_flush cmd_flush = {
    .hdr.type = VIRTIO_GPU_CMD_RESOURCE_FLUSH,
    .r = { .x = 0, .y = 0, .width = fb_width, .height = fb_height },
    .resource_id = RESOURCE_ID,
  };
  struct virtio_gpu_ctrl_hdr resp_flush;
  gpu_send_cmd(&cmd_flush, sizeof(cmd_flush), &resp_flush, sizeof(resp_flush));
}

void
gpu_draw_pixel(int x, int y, uint32 color)
{
  if (x < 0 || x >= fb_width || y < 0 || y >= fb_height)
    return;
  int offset = (y * fb_width + x) * 4;
  int page = offset / PGSIZE;
  int off = offset % PGSIZE;
  *(uint32*)((char*)fb_pages[page] + off) = color;
}

void
virtio_gpu_init(void)
{
  printf("Initializing GPU driver...\n");
  uint32 status = 0;
  
  initlock(&gpu.lock, "virtio_gpu");
  
  if (*R(VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976 ||
      *R(VIRTIO_MMIO_VERSION) != 2 ||
      *R(VIRTIO_MMIO_DEVICE_ID) != 16 ||  // GPU device ID
      *R(VIRTIO_MMIO_VENDOR_ID) != 0x554d4551) {
    panic("could not find virtio gpu");
  }
  
  // Reset
  *R(VIRTIO_MMIO_STATUS) = status;
  
  // Acknowledge
  status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
  *R(VIRTIO_MMIO_STATUS) = status;
  
  // Driver
  status |= VIRTIO_CONFIG_S_DRIVER;
  *R(VIRTIO_MMIO_STATUS) = status;
  
  // Negotiate features (none needed for basic 2D)
  *R(VIRTIO_MMIO_DRIVER_FEATURES) = 0;
  
  // Features OK
  status |= VIRTIO_CONFIG_S_FEATURES_OK;
  *R(VIRTIO_MMIO_STATUS) = status;
  
  // Setup virtqueue 0 (controlq)
  *R(VIRTIO_MMIO_QUEUE_SEL) = 0;
  uint32 max = *R(VIRTIO_MMIO_QUEUE_NUM_MAX);
  if (max == 0)
    panic("virtio gpu has no queue 0");
  if (max < NUM)
    panic("virtio gpu queue too short");
  *R(VIRTIO_MMIO_QUEUE_NUM) = NUM;
  
  // Allocate queue memory
  gpu.desc = kalloc();
  gpu.avail = kalloc();
  gpu.used = kalloc();
  memset(gpu.desc, 0, PGSIZE);
  memset(gpu.avail, 0, PGSIZE);
  memset(gpu.used, 0, PGSIZE);
  
  *R(VIRTIO_MMIO_QUEUE_DESC_LOW) = (uint64)gpu.desc;
  *R(VIRTIO_MMIO_QUEUE_DESC_HIGH) = (uint64)gpu.desc >> 32;
  *R(VIRTIO_MMIO_DRIVER_DESC_LOW) = (uint64)gpu.avail;
  *R(VIRTIO_MMIO_DRIVER_DESC_HIGH) = (uint64)gpu.avail >> 32;
  *R(VIRTIO_MMIO_DEVICE_DESC_LOW) = (uint64)gpu.used;
  *R(VIRTIO_MMIO_DEVICE_DESC_HIGH) = (uint64)gpu.used >> 32;
  
  *R(VIRTIO_MMIO_QUEUE_READY) = 1;
  
  for (int i = 0; i < NUM; i++)
    gpu.free[i] = 1;
  
  // Driver OK
  status |= VIRTIO_CONFIG_S_DRIVER_OK;
  *R(VIRTIO_MMIO_STATUS) = status;

  fb_width = FB_WIDTH;
  fb_height = FB_HEIGHT;
  
  for (int i = 0; i < FB_PAGES; i++) {
    fb_pages[i] = kalloc();
    if (!fb_pages[i])
      panic("virtio_gpu: kalloc failed");
    memset(fb_pages[i], 0, PGSIZE);
  }
  
  // Get display info
  struct virtio_gpu_ctrl_hdr cmd_info = { .type = VIRTIO_GPU_CMD_GET_DISPLAY_INFO };
  struct virtio_gpu_resp_display_info resp_info;
  memset(&resp_info, 0, sizeof(resp_info));
  gpu_send_cmd(&cmd_info, sizeof(cmd_info), &resp_info, sizeof(resp_info));

  // Create 2D resource
  struct virtio_gpu_resource_create_2d cmd_create;
  memset(&cmd_create, 0, sizeof(cmd_create));
  cmd_create.hdr.type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D;
  cmd_create.resource_id = RESOURCE_ID;
  cmd_create.format = VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM;
  cmd_create.width = fb_width;
  cmd_create.height = fb_height;

  struct virtio_gpu_ctrl_hdr resp_create;
  memset(&resp_create, 0, sizeof(resp_create));
  gpu_send_cmd(&cmd_create, sizeof(cmd_create), &resp_create, sizeof(resp_create));

  if (resp_create.type != VIRTIO_GPU_RESP_OK_NODATA) {
    panic("virtio_gpu: create 2d FAILED");
    return;
  }

  // Attach backing memory - must be done BEFORE scanout
  char *attach_buf = kalloc();
  memset(attach_buf, 0, PGSIZE);
  
  struct virtio_gpu_resource_attach_backing *attach_cmd = 
    (struct virtio_gpu_resource_attach_backing *)attach_buf;
  struct virtio_gpu_mem_entry *entries = 
    (struct virtio_gpu_mem_entry *)(attach_buf + sizeof(struct virtio_gpu_resource_attach_backing));
  
  attach_cmd->hdr.type = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING;
  attach_cmd->resource_id = RESOURCE_ID;
  attach_cmd->nr_entries = FB_PAGES;
  
  for (int i = 0; i < FB_PAGES; i++) {
    entries[i].addr = (uint64)fb_pages[i];
    entries[i].length = PGSIZE;
  }
  
  int attach_size = sizeof(struct virtio_gpu_resource_attach_backing) + 
                    FB_PAGES * sizeof(struct virtio_gpu_mem_entry);
  
  struct virtio_gpu_ctrl_hdr resp_attach;
  memset(&resp_attach, 0, sizeof(resp_attach));
  gpu_send_cmd(attach_buf, attach_size, &resp_attach, sizeof(resp_attach));

  if (resp_attach.type != VIRTIO_GPU_RESP_OK_NODATA) {
    panic("virtio_gpu: attach backing FAILED");
    return;
  }

  // Set scanout
  struct virtio_gpu_set_scanout cmd_scanout;
  memset(&cmd_scanout, 0, sizeof(cmd_scanout));
  cmd_scanout.hdr.type = VIRTIO_GPU_CMD_SET_SCANOUT;
  cmd_scanout.r.x = 0;
  cmd_scanout.r.y = 0;
  cmd_scanout.r.width = fb_width;
  cmd_scanout.r.height = fb_height;
  cmd_scanout.scanout_id = 0;
  cmd_scanout.resource_id = RESOURCE_ID;
  
  struct virtio_gpu_ctrl_hdr resp_scanout;
  memset(&resp_scanout, 0, sizeof(resp_scanout));
  gpu_send_cmd(&cmd_scanout, sizeof(cmd_scanout), &resp_scanout, sizeof(resp_scanout));

  // Fill screen with a color.
  for (int y = 0; y < fb_height; y++) {
    for (int x = 0; x < fb_width; x++) {
      gpu_draw_pixel(x, y, BACKGROUND_COLOR);  
    }
  }
  
  gpu_flush();
  
  printf("virtio_gpu: initialized %dx%d\n", fb_width, fb_height);
}


// Syscalls to be able to use the driver in userspace.

uint64 
sys_fbcopy(void) // Copy the userspace buffer to the gpu and render.
{
  uint64 ubuf; // Userspace buffer.
  argaddr(0, &ubuf);

  struct proc* p = myproc();

  for (int i = 0; i < FB_PAGES; i++)
  {
    if (copyin(p->pagetable, (char*)fb_pages[i], ubuf + i*PGSIZE, PGSIZE) < 0) 
    {
      return -1;
    }
  }

  gpu_flush();
  return 0;
}

uint64
sys_fbmap(void) // Map framebuffer to userspace.
{
  struct proc* p = myproc();

  uint64 vaddr = PGROUNDUP(p->sz); 

  for (int i = 0; i < FB_PAGES; i++) 
  {
    if (mappages(
        p->pagetable, 
        vaddr + i * PGSIZE, 
        PGSIZE, 
        (uint64)fb_pages[i], 
        PTE_R | PTE_W | PTE_U) != 0
    ) 
    {
      return -1;
    }
  }

  p->sz = vaddr + FB_PAGES * PGSIZE;

  return vaddr;
}


uint64
sys_fbflush(void) 
{
  gpu_flush();
  return 0;
}


