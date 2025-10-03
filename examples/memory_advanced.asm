# 高度なメモリ操作のサンプル
# リフターのメモリ操作機能をテスト

array_sum:
    # 配列の合計を計算
    # 配列のアドレス: %esi
    # 配列のサイズ: %ecx
    # 戻り値: %eax (合計)
    
    mov %eax, 0       # 合計を初期化
    mov %ebx, 0       # インデックスを初期化

sum_loop:
    cmp %ebx, %ecx    # インデックスとサイズを比較
    jge sum_done      # インデックス >= サイズ の場合、終了
    
    # 配列[インデックス]を読み込み
    mov %edx, (%esi+%ebx*4)  # %edx = 配列[インデックス]
    add %eax, %edx    # 合計に加算
    add %ebx, 1       # インデックスを増加
    jmp sum_loop      # ループの先頭に戻る

sum_done:
    ret               # 合計を返す

main:
    # 配列を初期化
    mov %esi, 1000    # 配列のベースアドレス
    mov %ecx, 5       # 配列のサイズ
    
    # 配列に値を設定
    mov (%esi), 10    # 配列[0] = 10
    mov (%esi+4), 20  # 配列[1] = 20
    mov (%esi+8), 30  # 配列[2] = 30
    mov (%esi+12), 40 # 配列[3] = 40
    mov (%esi+16), 50 # 配列[4] = 50
    
    # 配列の合計を計算
    call array_sum
    ret               # 結果: %eax = 150 (10+20+30+40+50)
