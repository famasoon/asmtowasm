# 高度な算術演算のサンプル
# リフターの機能をテストするための複雑な計算

complex_calculation:
    # 複数の変数を使用した計算
    mov %eax, 100     # %eax = 100
    mov %ebx, 25      # %ebx = 25
    mov %ecx, 10      # %ecx = 10
    
    # 複雑な式: (eax + ebx) * ecx - 50
    add %eax, %ebx    # %eax = 100 + 25 = 125
    mul %eax, %ecx    # %eax = 125 * 10 = 1250
    sub %eax, 50      # %eax = 1250 - 50 = 1200
    
    # 結果を別のレジスタにコピー
    mov %edx, %eax    # %edx = 1200
    
    ret               # 結果: %eax = 1200, %edx = 1200

main:
    call complex_calculation
    ret
