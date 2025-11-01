#define _POSIX_C_SOURCE 200809L // strdup을 위해
#include "my_assembler.h"

// 전역 변수 정의
char *input_data[MAX_LINES];
int line_num = 0;
token *token_table[MAX_LINES];
inst *inst_table[MAX_INST];
int inst_index = 0;


int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "사용법: %s <input_sic_file>\n", argv[0]);
        return 1;
    }

    // 1. inst.data 파일 로드
    load_inst_table("inst.data.txt");

    // 2. 입력 소스 파일 로드 및 파싱
    load_input_file(argv[1]);

    // 3. 파싱 결과 출력
    print_output();

    // 4. 메모리 해제
    cleanup();

    return 0;
}

void load_inst_table(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("inst.data 파일을 열 수 없습니다");
        exit(1);
    }

    char line[100];
    char name[10], type[10], opcode_str[10];
    int format;

    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "%s %s %d %s", name, type, &format, opcode_str) != 4) {
            continue; // 형식에 맞지 않는 라인 스킵
        }

        inst *new_inst = (inst *)malloc(sizeof(inst));
        if (!new_inst) {
            perror("메모리 할당 실패");
            exit(1);
        }

        strcpy(new_inst->str, name);
        new_inst->op = (unsigned char)strtol(opcode_str, NULL, 16); // 16진수 문자열을 바이트로 변환
        new_inst->format = format;
        new_inst->ops = get_operand_count_from_type(type); // Operand 개수 계산

        inst_table[inst_index++] = new_inst;
    }

    fclose(fp);
}

int get_operand_count_from_type(char *type_str) {
    // inst.data 및 inst_unit 명세 기반
    if (strcmp(type_str, "RR") == 0 || strcmp(type_str, "RN") == 0) {
        return 2; // (e.g., "R1,R2")
    }
    if (strcmp(type_str, "M") == 0 || strcmp(type_str, "R") == 0 || strcmp(type_str, "N") == 0) {
        return 1;
    }
    if (strcmp(type_str, "-") == 0) {
        return 0;
    }
    return 0; // (e.g., START, END, RESW 등)
}

void load_input_file(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("입력 파일을 열 수 없습니다");
        exit(1);
    }

    char line_buffer[256];
    while (fgets(line_buffer, sizeof(line_buffer), fp) && line_num < MAX_LINES) {
        // 한 줄씩 읽으면서 바로 파싱 함수 호출
        parse_line(line_num, line_buffer);
        line_num++;
    }

    fclose(fp);
}

void parse_line(int line_index, char *line) {
    // 1. token_unit 메모리 할당
    token *tok = (token *)malloc(sizeof(token));
    if (!tok) {
        perror("토큰 메모리 할당 실패");
        exit(1);
    }
    
    // 2. 토큰 구조체 초기화
    tok->label = NULL;
    tok->operator = NULL;
    tok->operand[0][0] = '\0';
    tok->operand[1][0] = '\0';
    tok->comment[0] = '\0';
    tok->is_comment_line = 0;
    
    token_table[line_index] = tok; // 전역 테이블에 연결

    // 3. 원본 라인 저장
    line[strcspn(line, "\n")] = 0; // 개행 문자 제거
    input_data[line_index] = strdup(line); // 원본 라인 복사본 저장
    
    // 4. 주석 라인 처리 ('*'로 시작)
    if (line[0] == '*') {
        tok->is_comment_line = 1;
        strcpy(tok->comment, line);
        return;
    }

    // 5. 토큰 분리 (strtok 사용)
    char *token_ptr;
    
    // 5-1. Label 처리 (첫 번째 문자가 공백이 아닌 경우)
    if (line[0] != ' ' && line[0] != '\t') {
        token_ptr = strtok(line, " \t");
        if (token_ptr) tok->label = strdup(token_ptr);
    } else {
        token_ptr = strtok(line, " \t"); // 공백인 경우 첫 토큰이 operator
    }

    // 5-2. Operator 처리
    if (!tok->label) { // 레이블이 없었으면
        token_ptr = strtok(line, " \t");
    } else { // 레이블이 있었으면
        token_ptr = strtok(NULL, " \t");
    }
    if (token_ptr) tok->operator = strdup(token_ptr);
    else return; // (Label만 있는 라인 등 예외 처리)

    // 5-3. Operand(s) 처리
    token_ptr = strtok(NULL, " \t\n");
    if (!token_ptr) return; // (e.g., RSUB 같이 Operand가 없는 명령어)

    // 5-4. 주석 처리 (간단하게 '.'으로 시작하는 부분)
    char *comment_start = strchr(token_ptr, '.');
    if (comment_start) {
        strcpy(tok->comment, comment_start);
        *comment_start = '\0'; // 피연산자 문자열에서 주석 부분 자르기
    }
    
    // 5-5. Operand 분리 (e.g., "TABLE,X" 또는 "R1,R2")
    char *op_tok = strtok(token_ptr, ",");
    int i = 0;
    while (op_tok && i < MAX_OPERAND) {
        strcpy(tok->operand[i++], op_tok);
        op_tok = strtok(NULL, ",");
    }
}

inst* find_opcode(char *operator_str) {
    if (!operator_str) return NULL;
    
    // inst_table에서 선형 탐색
    for (int i = 0; i < inst_index; i++) {
        if (strcmp(inst_table[i]->str, operator_str) == 0) {
            return inst_table[i];
        }
    }
    return NULL; // (e.g., START, END, RESW 등 Directive는 테이블에 없음)
}

void print_output(void) {
    printf("============================================================================================\n");
    printf("                                  SIC Parser Output\n");
    printf("============================================================================================\n");
    printf("%-35s | %-6s | %-10s | %-10s | %-15s | %s\n",
           "Source Line", "OPCODE", "Label", "Operator", "Operand(s)", "Comment");
    printf("-------------------------------------+--------+------------+------------+-----------------+----------\n");

    for (int i = 0; i < line_num; i++) {
        token *tok = token_table[i];
        
        // 원본 라인 출력
        printf("%-35s | ", input_data[i]);
        
        // 주석 라인인 경우
        if (tok->is_comment_line) {
            printf("COMMENT LINE\n");
            continue;
        }

        // OPCODE 출력
        inst *op_info = find_opcode(tok->operator);
        if (op_info) {
            printf("%02X     | ", op_info->op); // 2자리 16진수로 출력
        } else {
            printf("N/A    | "); // (Directive)
        }

        // 파싱된 토큰들 출력
        printf("%-10s | ", tok->label ? tok->label : " ");
        printf("%-10s | ", tok->operator ? tok->operator : " ");
        
        // Operand가 1개 또는 2개인 경우를 처리하여 하나의 문자열로 합침
        char operands_str[40] = {0};
        if (tok->operand[0][0] != '\0') {
            strcpy(operands_str, tok->operand[0]);
            if (tok->operand[1][0] != '\0') {
                strcat(operands_str, ",");
                strcat(operands_str, tok->operand[1]);
            }
        }
        printf("%-15s | ", operands_str[0] ? operands_str : " ");
        
        printf("%s\n", tok->comment[0] ? tok->comment : " ");
    }
    printf("============================================================================================\n");
}

void cleanup(void) {
    // inst_table 메모리 해제
    for (int i = 0; i < inst_index; i++) {
        free(inst_table[i]);
    }

    // input_data 및 token_table 메모리 해제
    for (int i = 0; i < line_num; i++) {
        free(input_data[i]); // strdup 해제
        if (token_table[i]) {
            free(token_table[i]->label);    // strdup 해제
            free(token_table[i]->operator); // strdup 해제
            free(token_table[i]);
        }
    }
}