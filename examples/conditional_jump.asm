# 条件分岐のサンプル
# 比較と条件分岐を使用

compare_values:
    mov %eax, 10      # %eax = 10
    mov %ebx, 15      # %ebx = 15
    
    cmp %eax, %ebx    # %eax と %ebx を比較
    
    jl less_than      # %eax < %ebx の場合、less_than にジャンプ
    jg greater_than   # %eax > %ebx の場合、greater_than にジャンプ
    je equal          # %eax == %ebx の場合、equal にジャンプ
    
less_than:
    mov %ecx, 1       # %ecx = 1 (less than)
    jmp end
    
greater_than:
    mov %ecx, 2       # %ecx = 2 (greater than)
    jmp end
    
equal:
    mov %ecx, 0       # %ecx = 0 (equal)
    
end:
    ret               # 結果: %ecx に比較結果が格納される
