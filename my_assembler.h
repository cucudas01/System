#ifndef MY_ASSEMBLER_H
#define MY_ASSEMBLER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// 상수 정의
#define MAX_LINES 5000
#define MAX_INST 256
#define MAX_OPERAND 2   // (e.g., "R1,R2")

// Instruction 테이블 관리 구조체
typedef struct inst_unit {
    char str[10];           // instruction의 이름
    unsigned char op;       // 명령어의 OPCODE
    int format;             // instruction의 형식
    int ops;                // instruction의 operand 개수
} inst;

// input 프로그램 관리 구조체
typedef struct token_unit {
    char *label;            // 명령어 라인 중 label
    char *operator;         // 명령어 라인 중 operator
    char operand[MAX_OPERAND][20]; // 명령어 라인 중 operand
    char comment[100];      // 명령어 라인 중 comment
    int loc;                // *** [추가] 이 라인의 Location Counter (LOC)
} token;

// *** [추가] Symbol Table (SYMTAB) 구조체 ***
typedef struct symtab_entry {
    char label[10];
    int loc;
} symtab;

// 전역 변수 선언
extern char *input_data[MAX_LINES];     // 원본 소스 코드 라인별 저장
extern int line_num;
extern token *token_table[MAX_LINES];
extern inst *inst_table[MAX_INST];
extern int inst_index;

// *** SYMTAB 전역 변수 선언 ***
extern symtab *symtab_table[MAX_INST]; // (편의상 MAX_INST 크기 재사용)
extern int symtab_index;

// --- 구현 함수 프로토타입 ---

void load_inst_table(const char *filename);
void load_input_file(const char *filename);
void parse_line(int line_index, char *line);
inst* find_opcode(char *operator_str);
void print_output(void);
void cleanup(void);
int get_operand_count_from_type(char *type_str);

// *** Pass 1 및 SYMTAB 함수 프로토타입 ***

/**
 * @brief token_table을 순회하며 LOC를 계산하고 SYMTAB을 구축합니다. (Pass 1)
 */
void execute_pass1(void);

/**
 * @brief SYMTAB에 새 심볼(레이블)을 추가합니다.
 * @param label 추가할 레이블
 * @param loc 해당 레이블의 메모리 주소(LOC)
 */
void add_to_symtab(char *label, int loc);

/**
 * @brief SYMTAB에서 레이블을 검색합니다.
 * @param label 검색할 레이블
 * @return 찾으면 해당 LOC, 못 찾으면 -1
 */
int find_in_symtab(char *label);


#endif // MY_ASSEMBLER_H