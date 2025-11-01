#define _POSIX_C_SOURCE 200809L // strdup을 위해
#include "my_assembler.h"

// 전역 변수 정의
char *input_data[MAX_LINES];
int line_num = 0;
token *token_table[MAX_LINES];
inst *inst_table[MAX_INST];
int inst_index = 0;

// SYMTAB 전역 변수 정의
symtab *symtab_table[MAX_INST];
int symtab_index = 0;


int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "사용법: %s <input_sic_file>\n", argv[0]);
        return 1;
    }

    // 1. inst.data 파일 로드
    load_inst_table("inst.data.txt");

    // 2. 입력 소스 파일 로드 및 파싱 (단순 토큰화)
    load_input_file(argv[1]);

    // 3. Pass 1 실행 (LOC 계산 및 SYMTAB 구축)
    execute_pass1();

    // 4. 파싱 및 Pass 2 결과 출력 (Object Code 생성 포함)
    print_output();

    // 5. 메모리 해제
    cleanup();

    return 0;
}

/**
 * @brief [신규] token_table을 순회하며 LOC를 계산하고 SYMTAB을 구축합니다. (Pass 1)
 */
void execute_pass1(void) {
    int loc_ctr = 0;

    // 1. 첫 번째 라인 (START) 특별 처리
    token* first_tok = token_table[0];
    if (first_tok->operator && strcmp(first_tok->operator, "START") == 0) {
        loc_ctr = (int)strtol(first_tok->operand[0], NULL, 16); // 16진수 문자열을 정수로
        first_tok->loc = loc_ctr;
    } else {
        loc_ctr = 0; // START가 없으면 0에서 시작
        first_tok->loc = 0;
    }
    
    // SYMTAB에 첫 라인 LABEL 추가
    if (first_tok->label) {
        add_to_symtab(first_tok->label, first_tok->loc);
    }

    // 2. 나머지 라인 순회
    for (int i = 1; i < line_num; i++) {
        token* tok = token_table[i];
        
        // 주석 라인 처리
        if (tok->comment[0] == '*') {
            tok->loc = -1; // 주석 라인 플래그
            continue;
        }

        // 현재 라인의 LOC는 이전 loc_ctr 값
        tok->loc = loc_ctr; 

        // SYMTAB에 LABEL 추가
        if (tok->label) {
            if (find_in_symtab(tok->label) != -1) {
                // 에러: 중복된 심볼
                printf("ERROR: Duplicate symbol '%s' at line %d\n", tok->label, i + 1);
                tok->loc = -2; // 에러 플래그
            } else {
                add_to_symtab(tok->label, loc_ctr);
            }
        }

        // *다음* 라인을 위한 loc_ctr 갱신
        if (tok->operator) {
            if (strcmp(tok->operator, "RESW") == 0) {
                loc_ctr += (atoi(tok->operand[0]) * 3);
            } else if (strcmp(tok->operator, "RESB") == 0) {
                loc_ctr += atoi(tok->operand[0]);
            } else if (strcmp(tok->operator, "BYTE") == 0) {
                if (tok->operand[0][0] == 'C') {
                    loc_ctr += (strlen(tok->operand[0]) - 3); // C'...' (양끝 ' 제외)
                } else if (tok->operand[0][0] == 'X') {
                    loc_ctr += (strlen(tok->operand[0]) - 3) / 2; // X'...' (16진수 2글자가 1바이트)
                }
            } else if (strcmp(tok->operator, "WORD") == 0) {
                loc_ctr += 3;
            } else if (strcmp(tok->operator, "END") == 0 || strcmp(tok->operator, "START") == 0) {
                // START는 이미 처리됨, END는 LOC 변경 없음
            } else {
                // 일반 명령어 (inst.data에 있든 없든, e.g., STZ)
                // (inst.data에 format이 있지만, 단순 SIC에서는 3바이트로 가정)
                loc_ctr += 3;
            }
        }
    }
}

/**
 * @brief [신규] SYMTAB에 새 심볼(레이블)을 추가합니다.
 */
void add_to_symtab(char *label, int loc) {
    if (symtab_index >= MAX_INST) {
        printf("SYMTAB is full.\n");
        return;
    }
    symtab *new_entry = (symtab *)malloc(sizeof(symtab));
    if (!new_entry) {
        perror("SYMTAB 메모리 할당 실패");
        return;
    }
    strcpy(new_entry->label, label);
    new_entry->loc = loc;
    symtab_table[symtab_index++] = new_entry;
}

/**
 * @brief [신규] SYMTAB에서 레이블을 검색합니다.
 */
int find_in_symtab(char *label) {
    for (int i = 0; i < symtab_index; i++) {
        if (strcmp(symtab_table[i]->label, label) == 0) {
            return symtab_table[i]->loc;
        }
    }
    return -1; // Not found
}

/**
 * @brief inst.data 파일을 읽어 inst_table을 초기화합니다.
 */
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
            continue;
        }
        inst *new_inst = (inst *)malloc(sizeof(inst));
        if (!new_inst) {
            perror("메모리 할당 실패");
            exit(1);
        }
        strcpy(new_inst->str, name);
        new_inst->op = (unsigned char)strtol(opcode_str, NULL, 16);
        new_inst->format = format;
        new_inst->ops = get_operand_count_from_type(type);
        inst_table[inst_index++] = new_inst;
    }
    fclose(fp);
}

/**
 * @brief inst.data의 operand type을 기반으로 operand 개수를 반환합니다.
 */
int get_operand_count_from_type(char *type_str) {
    if (strcmp(type_str, "RR") == 0 || strcmp(type_str, "RN") == 0) return 2;
    if (strcmp(type_str, "M") == 0 || strcmp(type_str, "R") == 0 || strcmp(type_str, "N") == 0) return 1;
    if (strcmp(type_str, "-") == 0) return 0;
    return 0;
}

/**
 * @brief 입력 SIC 소스 파일을 읽어 input_data와 token_table을 채웁니다.
 */
void load_input_file(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("입력 파일을 열 수 없습니다");
        exit(1);
    }
    char line_buffer[256];
    while (fgets(line_buffer, sizeof(line_buffer), fp) && line_num < MAX_LINES) {
        parse_line(line_num, line_buffer);
        line_num++;
    }
    fclose(fp);
}

/**
 * @brief 소스 코드 한 라인을 파싱하여 token_table에 저장합니다. (LOC 계산은 안 함)
 */
void parse_line(int line_index, char *line) {
    token *tok = (token *)malloc(sizeof(token));
    if (!tok) {
        perror("토큰 메모리 할당 실패");
        exit(1);
    }
    
    // 토큰 구조체 초기화
    tok->label = NULL;
    tok->operator = NULL;
    tok->operand[0][0] = '\0';
    tok->operand[1][0] = '\0';
    tok->comment[0] = '\0';
    tok->loc = 0; // LOC 초기화 (execute_pass1에서 덮어쓸 것임)
    
    token_table[line_index] = tok; 

    line[strcspn(line, "\n")] = 0; // 개행 문자 제거
    input_data[line_index] = strdup(line); // 원본 라인 복사본 저장
    
    char line_copy[256];
    strcpy(line_copy, line); // strtok가 원본을 수정하므로 복사본 사용

    // 주석 라인 처리 ('*'로 시작)
    if (line[0] == '*') {
        strcpy(tok->comment, line);
        return;
    }

    char *token_ptr;
    
    // Label 처리
    if (line[0] != ' ' && line[0] != '\t') {
        token_ptr = strtok(line_copy, " \t");
        if (token_ptr) tok->label = strdup(token_ptr);
        token_ptr = strtok(NULL, " \t\n"); // 다음 토큰 (Operator)
    } else {
        token_ptr = strtok(line_copy, " \t\n"); // 첫 토큰 (Operator)
    }

    // Operator 처리
    if (token_ptr) tok->operator = strdup(token_ptr);
    else return; 

    // Operand(s) + Comment 처리
    token_ptr = strtok(NULL, "\n");
    if (!token_ptr) return; 

    while (*token_ptr == ' ' || *token_ptr == '\t') token_ptr++;
    if (*token_ptr == '\0') return; 

    // 주석 처리 ('.'으로 시작하는 부분)
    char *comment_start = strchr(token_ptr, '.');
    if (comment_start) {
        strcpy(tok->comment, comment_start);
        *comment_start = '\0';
    }
    
    // Operand 문자열의 뒤쪽 공백 제거
    int len = strlen(token_ptr);
    while (len > 0 && (token_ptr[len - 1] == ' ' || token_ptr[len - 1] == '\t')) {
        token_ptr[--len] = '\0';
    }
    if (len == 0) return; 
    
    // Operand 분리
    char *op_tok = strtok(token_ptr, ",");
    int i = 0;
    while (op_tok && i < MAX_OPERAND) {
        while (*op_tok == ' ' || *op_tok == '\t') op_tok++;
        int op_len = strlen(op_tok);
        while (op_len > 0 && (op_tok[op_len-1] == ' ' || op_tok[op_len-1] == '\t')) {
            op_tok[--op_len] = '\0';
        }
        strcpy(tok->operand[i++], op_tok);
        op_tok = strtok(NULL, ",");
    }
}

/**
 * @brief operator 문자열을 inst_table에서 찾아 해당 inst_unit 포인터를 반환합니다.
 */
inst* find_opcode(char *operator_str) {
    if (!operator_str) return NULL;
    for (int i = 0; i < inst_index; i++) {
        if (strcmp(inst_table[i]->str, operator_str) == 0) {
            return inst_table[i];
        }
    }
    return NULL;
}

// [신규] Object Code 생성을 위한 static 버퍼
static char obj_code_result[10];

/**
 * @brief [신규] 토큰 정보를 기반으로 완전한 Object Code를 생성합니다. (Pass 2)
 */
char* generate_object_code(token* tok) {
    strcpy(obj_code_result, ""); // 버퍼 초기화

    if (!tok->operator) {
        return obj_code_result;
    }

    // --- 1. Handle Directives (지시어 처리) ---
    if (strcmp(tok->operator, "START") == 0 || strcmp(tok->operator, "END") == 0 ||
        strcmp(tok->operator, "RESW") == 0 || strcmp(tok->operator, "RESB") == 0) {
        return obj_code_result; // Object Code 없음
    }

    if (strcmp(tok->operator, "WORD") == 0) {
        int val = atoi(tok->operand[0]);
        sprintf(obj_code_result, "%06X", val); // e.g., 3 -> "000003"
        return obj_code_result;
    }

    if (strcmp(tok->operator, "BYTE") == 0) {
        if (tok->operand[0][0] == 'C') {
            // e.g., C'EOF' -> 454F46
            char str_val[10] = {0};
            strncpy(str_val, tok->operand[0] + 2, strlen(tok->operand[0]) - 3);
            for (int i = 0; i < strlen(str_val); i++) {
                sprintf(obj_code_result + (i*2), "%02X", str_val[i]);
            }
        } else if (tok->operand[0][0] == 'X') {
            // e.g., X'F1' -> F1
            strncpy(obj_code_result, tok->operand[0] + 2, strlen(tok->operand[0]) - 3);
        }
        return obj_code_result;
    }

    // --- 2. Handle Instructions (명령어 처리) ---
    inst *op_info = find_opcode(tok->operator);
    if (!op_info) {
        // e.g., STZ (inst.data에 없는 명령어)
        return obj_code_result; // Object Code 없음 ("")
    }

    int opcode = op_info->op;
    int operand_addr = 0;

    // Handle RSUB (no operand)
    if (strcmp(tok->operator, "RSUB") == 0) {
        sprintf(obj_code_result, "%02X0000", opcode); // 4C0000
        return obj_code_result;
    }

    // Handle other instructions with operands
    if (tok->operand[0][0] != '\0') {
        char *operand_str = tok->operand[0];

        // Check for immediate addressing (e.g., #0, #3)
        if (operand_str[0] == '#') {
            operand_addr = atoi(operand_str + 1);
            // No indexing possible with immediate addressing
        } else {
            // Symbolic addressing (e.g., LENGTH, BUFFER, ZERO)
            operand_addr = find_in_symtab(operand_str);
            if (operand_addr == -1) {
                // Operand가 SYMTAB에 없는 경우 (e.g., 4096)
                // (SIC/XE가 아니므로, 이는 보통 숫자 리터럴이거나 오류임)
                // 여기서는 MAXLEN (4096)을 처리하기 위해 atoi 시도
                int literal_val = atoi(operand_str);
                if (literal_val != 0 || strcmp(operand_str, "0") == 0) {
                     operand_addr = literal_val;
                } else {
                    strcpy(obj_code_result, "SYMTAB ERR"); // Undefined symbol
                    return obj_code_result;
                }
            }

            // Check for indexed addressing (e.g., BUFFER,X)
            if (tok->operand[1][0] == 'X' || tok->operand[1][0] == 'x') {
                operand_addr |= 0x8000; // Set X bit (add 32768)
            }
        }
    }
    
    // Combine opcode and address
    int object_code_val = (opcode << 16) + operand_addr;
    sprintf(obj_code_result, "%06X", object_code_val);

    return obj_code_result;
}


/**
 * @brief [수정됨] 파싱 및 Pass 2 결과를 출력합니다. (Full Object Code, SYMTAB 출력 X)
 */
void print_output(void) {
    char* obj_code_str; // Object code를 저장할 포인터

    printf("======================================================================================\n");
    printf("                                SIC Pass 1/2 Output\n"); // 타이틀 변경
    printf("======================================================================================\n");

    printf("%-5s | %-6s | %-50s | %s\n",
           "Line", "LOC", "Source Statement", "Object Code"); // 헤더 변경
    printf("------+--------+----------------------------------------------------+------------------\n");

    for (int i = 0; i < line_num; i++) {
        token *tok = token_table[i];

        // 1. Line (줄 번호) 출력
        printf("%-5d | ", (i+1) *5);

        // 2. LOC (위치 카운터) - 계산된 LOC 출력
        if (tok->loc == -1) { // 주석
            printf("       | ");
        } else if (tok->loc == -2) { // 에러
            printf("ERROR  | ");
        } else {
            printf("%04X   | ", tok->loc); // 4자리 16진수로 출력
        }

        // 3. Source Statement (원본 라인) 출력
        printf("%-50s | ", input_data[i]);
        
        // 주석 라인인 경우
        if (tok->loc == -1) { // tok->comment[0] == '*' 대신 loc 플래그 사용
            printf("COMMENT LINE\n");
            continue;
        }

        // 4. Object Code (Pass 2)
        // [수정] generate_object_code() 호출
        obj_code_str = generate_object_code(tok);
        printf("%s\n", obj_code_str);
    }
    printf("======================================================================================\n");

    // [제거] SYMTAB 출력 루프
    /*
    printf("\n======================== SYMBOL TABLE (SYMTAB) ========================\n");
    printf("%-10s | %-6s\n", "Label", "LOC");
    printf("-----------+--------\n");
    for (int i = 0; i < symtab_index; i++) {
        printf("%-10s | %04X\n", symtab_table[i]->label, symtab_table[i]->loc);
    }
    printf("=======================================================================\n");
    */
}

/**
 * @brief 동적으로 할당된 모든 메모리를 해제합니다. (SYMTAB 포함)
 */
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

    // SYMTAB 메모리 해제 (출력은 안 하지만, 해제는 해야 함)
    for (int i = 0; i < symtab_index; i++) {
        free(symtab_table[i]);
    }
}
