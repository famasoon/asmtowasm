# ループの例
# 1から10までの合計を計算

sum_loop:
    mov %eax, 0        # 合計を初期化
    mov %ebx, 1        # カウンタを初期化
    mov %ecx, 10       # 上限を設定

loop_start:
    cmp %ebx, %ecx     # カウンタと上限を比較
    jg loop_end        # カウンタ > 上限 の場合、ループ終了
    
    add %eax, %ebx     # 合計にカウンタを加算
    add %ebx, 1        # カウンタを増加
    jmp loop_start     # ループの先頭に戻る

loop_end:
    ret               # 結果: %eax = 55 (1+2+...+10)

main:
    call sum_loop
    ret
