;生成ELF可从定位目标文件
;nasm -f elf hello.asm -o hello.o
;链接成ELF可执行目标文件
;ld -s hello.o -o hello

[section .data]

strHello db "hello,world!", 0Ah
strLen equ $ - strHello

[section .text]

global _start

_start:
	mov edx, strLen
	mov ecx, strHello
	mov ebx, 1
	mov eax, 4
	int 0x80

	mov ebx, 0
	mov eax, 1
	int 0x80
