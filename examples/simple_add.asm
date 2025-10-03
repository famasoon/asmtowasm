# 簡単な加算プログラムのサンプル
# レジスタ %eax と %ebx に値を設定し、加算する

start:
    mov %eax, 10      # %eax = 10
    mov %ebx, 20      # %ebx = 20
    add %eax, %ebx    # %eax = %eax + %ebx (結果: 30)
    ret               # 関数から戻る
