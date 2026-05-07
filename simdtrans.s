    .global simdtrans
    .text

    .macro transpose2x2x1 rfrom:req, offset:req, regto1:req, regto2:req
        vmovdqa    \offset(\rfrom), %ymm2
        vmovdqa    \offset + 0x20(\rfrom), %ymm3

        vpunpcklbw %ymm3, %ymm2, %ymm0
        vpunpckhbw %ymm3, %ymm2, %ymm1

        vpunpcklwd %ymm1, %ymm0, \regto1
        vpunpckhwd %ymm1, %ymm0, \regto2
    .endm

    .macro transpose2x2x2 r1:req, r2:req
        vpunpcklwd \r2, \r1, %ymm0
        vpunpckhwd \r2, \r1, %ymm1

        vpunpckldq %ymm1, %ymm0, \r1
        vpunpckhdq %ymm1, %ymm0, \r2
    .endm

    .macro transpose2x2x4 r1:req, r2:req
        vpunpckldq  \r2, \r1, %ymm0
        vpunpckhdq  \r2, \r1, %ymm1

        vpunpcklqdq %ymm1, %ymm0, \r1
        vpunpckhqdq %ymm1, %ymm0, \r2
    .endm

    # Swap high 128 bits of one ymm register with low 128 bits of another
    .macro swaphldqw r1N:req, r2N:req
        vperm2i128 $0x20, %ymm\r2N, %ymm\r1N, %ymm0
        vperm2i128 $0x31, %ymm\r2N, %ymm\r1N, %ymm\r2N
        vmovdqa    %ymm0, %ymm\r1N
    .endm

simdtrans:
    transpose2x2x1 %rdi, 0x00, %ymm4,  %ymm5
    transpose2x2x1 %rdi, 0x40, %ymm6,  %ymm7
    transpose2x2x1 %rdi, 0x80, %ymm8,  %ymm9
    transpose2x2x1 %rdi, 0xc0, %ymm10, %ymm11

    transpose2x2x2 %ymm4, %ymm6
    transpose2x2x2 %ymm5, %ymm7
    transpose2x2x2 %ymm8, %ymm10
    transpose2x2x2 %ymm9, %ymm11

    transpose2x2x4 %ymm4, %ymm8
    transpose2x2x4 %ymm5, %ymm9
    transpose2x2x4 %ymm6, %ymm10
    transpose2x2x4 %ymm7, %ymm11

    swaphldqw 4, 6
    swaphldqw 8, 10
    swaphldqw 5, 7
    swaphldqw 9, 11

    vpunpcklbw   %ymm6, %ymm4, %ymm1
    vmovdqa      %ymm1, 0x00(%rdi)
    vpunpcklbw   %ymm10, %ymm8, %ymm1
    vmovdqa      %ymm1, 0x20(%rdi)
    vpunpcklbw   %ymm7, %ymm5, %ymm1
    vmovdqa      %ymm1, 0x40(%rdi)
    vpunpcklbw   %ymm11, %ymm9, %ymm1
    vmovdqa      %ymm1, 0x60(%rdi)

    vpunpckhbw   %ymm6, %ymm4, %ymm1
    vmovdqa      %ymm1, 0x80(%rdi)
    vpunpckhbw   %ymm10, %ymm8, %ymm1
    vmovdqa      %ymm1, 0xa0(%rdi)
    vpunpckhbw   %ymm7, %ymm5, %ymm1
    vmovdqa      %ymm1, 0xc0(%rdi)
    vpunpckhbw   %ymm11, %ymm9, %ymm1
    vmovdqa      %ymm1, 0xe0(%rdi)

    ret
