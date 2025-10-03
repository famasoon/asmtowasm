# 算術演算のサンプル
# 基本的な四則演算を実行

main:
    mov %eax, 100     # %eax = 100
    mov %ebx, 25      # %ebx = 25
    
    # 加算
    add %eax, %ebx    # %eax = 100 + 25 = 125
    
    # 減算
    mov %ecx, 50      # %ecx = 50
    sub %eax, %ecx    # %eax = 125 - 50 = 75
    
    # 乗算
    mov %edx, 2       # %edx = 2
    mul %eax, %edx    # %eax = 75 * 2 = 150
    
    # 除算
    mov %esi, 3       # %esi = 3
    div %eax, %esi    # %eax = 150 / 3 = 50
    
    ret               # 結果: %eax = 50
