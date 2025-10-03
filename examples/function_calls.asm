# 関数呼び出しのサンプル
# リフターの関数呼び出し機能をテスト

# 階乗を計算する関数
factorial:
    # 引数: %eax (n)
    # 戻り値: %eax (n!)
    
    # ベースケース: n <= 1
    cmp %eax, 1
    jle base_case
    
    # 再帰ケース: n * factorial(n-1)
    mov %ebx, %eax    # %ebx = n
    sub %eax, 1       # %eax = n-1
    call factorial    # %eax = factorial(n-1)
    mul %eax, %ebx    # %eax = factorial(n-1) * n
    ret

base_case:
    mov %eax, 1       # factorial(1) = 1
    ret

# メイン関数
main:
    mov %eax, 5       # factorial(5)を計算
    call factorial
    ret               # 結果: %eax = 120 (5!)
