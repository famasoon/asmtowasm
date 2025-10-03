# フィボナッチ数列の計算
# 再帰的な実装

fibonacci:
    # 引数: %eax (n)
    # 戻り値: %eax (fibonacci(n))
    
    # ベースケース: n <= 1 の場合
    mov %ebx, 1
    cmp %eax, %ebx
    jle base_case
    
    # 再帰ケース: fibonacci(n-1) + fibonacci(n-2)
    push %eax          # nを保存
    sub %eax, 1        # n-1
    call fibonacci     # fibonacci(n-1)
    mov %ecx, %eax     # 結果を%ecxに保存
    
    pop %eax           # nを復元
    push %ecx          # fibonacci(n-1)を保存
    sub %eax, 2        # n-2
    call fibonacci     # fibonacci(n-2)
    mov %edx, %eax     # 結果を%edxに保存
    
    pop %ecx           # fibonacci(n-1)を復元
    add %ecx, %edx     # fibonacci(n-1) + fibonacci(n-2)
    mov %eax, %ecx     # 結果を%eaxに格納
    ret

base_case:
    # fibonacci(0) = 0, fibonacci(1) = 1
    cmp %eax, 0
    je zero_case
    mov %eax, 1        # fibonacci(1) = 1
    ret

zero_case:
    mov %eax, 0        # fibonacci(0) = 0
    ret

main:
    mov %eax, 10       # fibonacci(10)を計算
    call fibonacci
    ret               # 結果: 55
