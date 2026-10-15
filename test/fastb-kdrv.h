/* shared by the driver and the probe of the fastb-kernel gate */
#ifndef FASTB_KDRV_H
#define FASTB_KDRV_H

#define FASTB_IOCTL_SELF  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x900, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define FASTB_IOCTL_COPY  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x901, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define FASTB_IOCTL_PRIO  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x902, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define SELF_CHECKS(X) \
    X(irql_raise_from_passive) X(irql_raise_returns_dispatch) X(irql_lower_to_dispatch) X(irql_lower_to_passive) \
    X(irql_get_after_raise) X(irql_raise_dpc_level_returns_old) X(irql_raise_synch_level) \
    X(spin_acquire_old_irql) X(spin_held_at_dispatch) X(spin_release_restores) X(spin_nested_old_apc) \
    X(spin_nested_restores_apc) X(queued_spin_old_irql) X(queued_spin_held_at_dispatch) X(queued_spin_release_restores) \
    X(probe_read_valid) X(probe_write_valid) X(probe_read_misaligned) X(probe_write_misaligned) X(probe_align1_odd_ok) \
    X(probe_read_kernel_range) X(probe_write_kernel_range) X(probe_read_wraparound) X(probe_zero_length_kernel_ok) \
    X(probe_write_readonly_const) X(probe_read_readonly_const_ok) X(probe_write_second_page_readonly) \
    X(probe_read_second_page_readonly_ok) X(probe_write_keeps_data) X(probe_write_last_byte_page) \
    X(contig_alloc_ok) X(contig_page_aligned) X(contig_below_4gb) X(contig_writable) X(contig_physical_identity) \
    X(contig_impossible_range_null) X(contig_cache_noncached_ok) X(contig_bad_cache_type_null) X(contig_boundary_crossing_null) \
    X(contig_boundary_ok) X(contig_low_bound_null) X(contig_zero_size_null) \
    X(mdl_pages_alloc) X(mdl_pages_bytecount) X(mdl_pages_pfn_identity) X(mdl_pages_map_returns_startva) \
    X(mdl_pages_writable) X(mdl_pages_unaligned_size_null) X(mdl_pages_free) X(mdl_nonpaged_pfn)

#define X(n) R_##n,
enum self_check { SELF_CHECKS(X) NUM_SELF_CHECKS };
#undef X

struct self_results
{
    unsigned int done;                  /* NUM_SELF_CHECKS when all were run */
    unsigned int code[NUM_SELF_CHECKS]; /* the exception code seen by the checks that raise, else 0 */
    unsigned char ok[NUM_SELF_CHECKS];
};

#define COPY_CHECKS(X) \
    X(copy_from_other_status) X(copy_from_other_count) X(copy_from_other_data) X(copy_to_other_status) \
    X(copy_to_other_count) X(copy_zero_length) X(copy_unmapped_partial) X(copy_unmapped_count) \
    X(copy_same_process) X(copy_same_process_data) X(copy_usermode_kernel_address) X(copy_usermode_kernel_count)

#define X(n) C_##n,
enum copy_check { COPY_CHECKS(X) NUM_COPY_CHECKS };
#undef X

struct copy_input
{
    unsigned long long pid;
    unsigned long long src;     /* the 256 byte pattern in the other process */
    unsigned long long dst;     /* 256 bytes of scratch in the other process */
};

struct copy_results
{
    unsigned int done;
    unsigned char ok[NUM_COPY_CHECKS];
};
#endif
