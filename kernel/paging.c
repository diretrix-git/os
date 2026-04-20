#include "paging.h"
#include "pmm.h"
#include "vga.h"

static struct page_directory *kernel_directory = 0;
static struct page_directory *current_directory = 0;

static inline void invlpg(uint32_t addr)
{
    __asm__ volatile("invlpg (%0)" : : "r"(addr) : "memory");
}

static inline void set_cr3(uint32_t addr)
{
    __asm__ volatile("mov %0, %%cr3" : : "r"(addr));
}

static inline void enable_paging_bit(void)
{
    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));
}

/*
 * Identity-map the first 16 MB so that physical == virtual for all
 * kernel code/data.  We allocate page tables from the PMM.
 */
void paging_init(void)
{
    /* Allocate page directory (1 frame = 4 KB) */
    uint32_t pd_phys = pmm_alloc_frame();
    kernel_directory = (struct page_directory *)pd_phys;
    current_directory = kernel_directory;

    /* Zero the directory */
    uint32_t *pd = (uint32_t *)pd_phys;
    for (int i = 0; i < 1024; i++)
        pd[i] = 0;

    /* Identity-map 0 – 16 MB (4 page tables × 4 MB each) */
    for (int t = 0; t < 4; t++)
    {
        uint32_t pt_phys = pmm_alloc_frame();
        uint32_t *pt = (uint32_t *)pt_phys;

        for (int e = 0; e < 1024; e++)
        {
            uint32_t phys = (t * 1024 + e) * PAGE_SIZE;
            pt[e] = phys | PAGE_PRESENT | PAGE_WRITE;
        }

        pd[t] = pt_phys | PAGE_PRESENT | PAGE_WRITE;
    }

    set_cr3(pd_phys);
    enable_paging_bit();
}

void map_page(uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags)
{
    uint32_t pd_index = (virtual_addr >> 22) & 0x3FF;
    uint32_t pt_index = (virtual_addr >> 12) & 0x3FF;

    uint32_t *pd = (uint32_t *)current_directory;

    if (!(pd[pd_index] & PAGE_PRESENT))
    {
        uint32_t pt_phys = pmm_alloc_frame();
        uint32_t *pt = (uint32_t *)pt_phys;
        for (int i = 0; i < 1024; i++)
            pt[i] = 0;
        pd[pd_index] = pt_phys | PAGE_PRESENT | PAGE_WRITE;
    }

    uint32_t pt_phys = pd[pd_index] & ~0xFFF;
    uint32_t *pt = (uint32_t *)pt_phys;
    pt[pt_index] = physical_addr | flags;
    invlpg(virtual_addr);
}

void unmap_page(uint32_t virtual_addr)
{
    uint32_t pd_index = (virtual_addr >> 22) & 0x3FF;
    uint32_t pt_index = (virtual_addr >> 12) & 0x3FF;

    uint32_t *pd = (uint32_t *)current_directory;
    if (!(pd[pd_index] & PAGE_PRESENT))
        return;

    uint32_t pt_phys = pd[pd_index] & ~0xFFF;
    uint32_t *pt = (uint32_t *)pt_phys;
    pt[pt_index] = 0;
    invlpg(virtual_addr);
}

void page_fault_handler(uint32_t error_code, uint32_t faulting_address)
{
    vga_print("\n!!! PAGE FAULT !!!\n");
    vga_print("Faulting address: 0x");
    vga_print_hex(faulting_address);
    vga_print("\nError code: 0x");
    vga_print_hex(error_code);
    vga_print("\n");
    while (1)
        __asm__ volatile("hlt");
}