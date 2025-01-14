.global cas

# a0 holds address of memory location
# a1 holds expected value
# a2 holds desired value
# a0 holds return value, 0 if successful, !0 otherwise

cas:
    lr.w t0, (a0) 
    bne t0, a1, fail 
    sc.w a0, a2, (a0) 
    jr ra 
fail:
    li a0, 1 
    jr ra