#ifndef MY_ASSEMBLER_H
#define MY_ASSEMBLER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// 상수 정의
#define MAX_LINES 5000
#define MAX_INST 256
#define MAX_OPERAND 2

// --- PDF 명세서 1. input 프로그램 관리 ---
// (PDF의 문법 오류를 수정한 버전입니다)
typedef struct token_unit {
    char *label;            // 명령어 라인 중 label
    char *operator;         // 명령어 라인 중 operator
    char operand[MAX_OPERAND][20]; // 명령어 라인 중 operand
    char comment[100];      // 명령어 라인 중 comment
} token;


// --- PDF 명세서 2. Instruction 테이블 관리 ---
// (PDF의 문법 오류를 수정한 버전입니다)
typedef struct inst_unit {
    char str[10];           // instruction의 이름
    unsigned char op;       // 명령어의 OPCODE
    int format;             // instruction의 형식
    int ops;                // instruction의 operand 개수
} inst;

// --- 전역 변수 선언 ---
extern char *input_data[MAX_LINES];     // 원본 소스 코드 라인별 저장
extern int line_num;
extern token *token_table[MAX_LINES];
extern inst *inst_table[MAX_INST];
extern int inst_index;


// --- 구현 함수 프로토타입 ---

/**
 * @brief inst.data 파일을 읽어 inst_table을 초기화합니다.
 * @param filename inst.data 파일 경로
 */
void load_inst_table(const char *filename);

/**
 * @brief 입력 SIC 소스 파일을 읽어 input_data와 token_table을 채웁니다.
 * @param filename 입력 SIC 소스 파일 경로
 */
void load_input_file(const char *filename);

/**
 * @brief 소스 코드 한 라인을 파싱하여 token_table에 저장합니다.
 * @param line_index 현재 라인 번호
 * @param line 원본 라인 문자열
 */
void parse_line(int line_index, char *line);

/**
 * @brief operator 문자열을 inst_table에서 찾아 해당 inst_unit 포인터를 반환합니다.
 * @param operator_str 찾고자 하는 operator (e.g., "LDA")
 * @return 찾은 inst_unit 포인터, 없으면 NULL
 */
inst* find_opcode(char *operator_str);

/**
 * @brief 파싱 결과를 명세서 요구사항에 맞게 출력합니다.
 */
void print_output(void);

/**
 * @brief 동적으로 할당된 모든 메모리를 해제합니다.
 */
void cleanup(void);

/**
 * @brief inst.data의 operand type (e.g., "M", "RR")을 기반으로
 * operand 개수(ops)를 반환합니다.
 * @param type_str "M", "RR", "R", "N", "RN", "-" 등
 * @return operand 개수
 */
int get_operand_count_from_type(char *type_str);

#endif // MY_ASSEMBLER_H