# メモリ操作の例
# 配列の要素にアクセスして計算

array_operations:
    # 配列のアドレスを%esiに設定（簡単化のため固定値）
    mov %esi, 1000     # 配列のベースアドレス
    
    # 配列[0] = 10
    mov %eax, 10
    mov (%esi), %eax   # 配列[0]に10を格納
    
    # 配列[1] = 20
    mov %eax, 20
    mov (%esi+4), %eax # 配列[1]に20を格納
    
    # 配列[2] = 30
    mov %eax, 30
    mov (%esi+8), %eax # 配列[2]に30を格納
    
    # 配列[0] + 配列[1] + 配列[2] を計算
    mov %eax, (%esi)      # 配列[0]を読み込み
    mov %ebx, (%esi+4)    # 配列[1]を読み込み
    add %eax, %ebx        # 配列[0] + 配列[1]
    
    mov %ebx, (%esi+8)    # 配列[2]を読み込み
    add %eax, %ebx        # 合計: 10 + 20 + 30 = 60
    
    ret               # 結果: %eax = 60

main:
    call array_operations
    ret
