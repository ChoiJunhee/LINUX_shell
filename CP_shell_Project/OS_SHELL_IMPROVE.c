/*
 * <제출물 : 소스코드, 보고서>
 * 보고서에는 프로그램의 동작에 대한 설명,
 * 잘 구현된 부분, 어려웠던 부분이 포함되어야 함
 *
 * 소스코드(.c), 보고서 (docx 또는 hwp) 각각 제출 (압축 금지)

*/

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#define MAX_ARG 16
#define MAX_LINE 128

/* 중요한거 */
char *exit_str = "exit";
char *back_str = "&";
int stat_background = 0;
int back_count = 1;

/* 함수 선언 */
int input_tokenization(char prompt[], char* cmd_list[]);
int operator_check(char *word);
void command_execute(int num, char *cmd_list[]);
void shell_execute(int flag, int cnt1, int cnt2, char *list1[], char *list2[], int Background);
void input_redirect_owrite(int cnt1, int cnt2, char *list1[], char *list2[], int Background);
void input_redirect_append(int cnt1, int cnt2, char *list1[], char *list2[], int Background);
void output_redirect(int cnt1, int cnt2, char *list1[], char *list2[], int Background);
void single_pipe(int cnt1, int cnt2, char *list1[], char *list2[], int Background);
void double_pipe(int cnt1, int cnt2, char *list1[], char *list2[], int Background);


/* 메 인 함 수 */
int main(){
	char prompt[MAX_LINE];
	char buf[MAX_LINE];
	char *cmd_list[MAX_ARG];
	int CMD_CNT, status;
	pid_t pid;

	while(1){
		/* 자식 프로세스 (프롬프트)의 exit 명령 확인용 */
		int exit_pipe[2];
		if (pipe(exit_pipe) < 0) {
			printf("$ 파이프 생성 오류\n");
			exit(1);
		}

		if ((pid = fork()) < 0) { /* 자식 프로세스 생성 */
			printf("Custom Shell $ 자식 프로세스 생성 오류\n");
			exit(1);
		}
		else if (pid == 0) { /* 자식 프로세스 */
			close(exit_pipe[0]); // 파이프 READ 닫음
			printf("Custom Shell $ ");

			/* 문장 입력 받음 */
			memset(prompt, 0, sizeof(char) * MAX_LINE); // 프롬프트 버퍼 초기화
			if (!fgets(prompt, MAX_LINE, stdin)) break;
			fflush(stdin); // 표준 입력 스트림 버퍼 초기화

			/* 토큰화 진행 및 명령어 개수 확인 */
			CMD_CNT = input_tokenization(prompt, cmd_list);
			close(exit_pipe[1]); // 파이프 WRITE 닫음

			/* 명령어 실행하러 ㄱㄱ */
			command_execute(CMD_CNT, cmd_list);
			exit(0);
		} 
		else { /* 부모 프로세스 */

			/* EXIT 입력 확인 (파이프) */
			close(exit_pipe[1]);
			memset(buf, 0, sizeof(char) * MAX_LINE); //버퍼 초기화

			if (read(exit_pipe[0], buf, sizeof(char) * 4) < 0) {
				printf("파이프 읽기 오류\n");
			}
			close(exit_pipe[0]);

			/* EXIT이 확인되면 부모 프로세스도 종료 (프로그램 종료) */
			if (!strcmp(buf, exit_str)) exit(0);

			/* Background 옵션이 확인되면 wait 변경 */
			if (!strcmp(buf, back_str)) {
				printf("[%d] %d\n", back_count, getpid());
				waitpid(pid, &status, WNOHANG);
				back_count++;
			} else {
				wait(&status);
			}
		}
		back_count--;
	}
	return 0;
}

/* 프롬프트로 입력받은 문자열을 기준에 맞게 정렬하는 함수 */
int input_tokenization(char prompt[], char *cmd_list[]){
	/* 프롬프트 스트림에 있는 \n 을 잘라냄 */
	if(prompt[strlen(prompt)-1] == '\n') prompt[strlen(prompt)-1] = '\0';

	/* EXIT 입력인 경우 */
	if(!strcmp(prompt, "exit")){
		cmd_list[0] = prompt;
		return -1;
	}

	/* (, ), ; 을 구분하여 '순서'를 확인하자 */
	/* char *scan_str[3] = {"(", ")", ";"}; */
	/* &, ;,는 토큰화 과정에서 제외함 */

	char *Ready_Queue[MAX_ARG];
	char *Token_Queue[MAX_ARG];
	int Ready_Queue_Count = 0;
	int Token_Queue_Count = 0;
	int CMD_LIST_COUNT = 0;
	
	int i=0, flag = 0; // 0 : 정상, 1 : 소괄호, 2 : 세미콜론 

	char *ptr = strtok(prompt, " ");
	while(1){

		/* 띄어쓰기 만 고려하여 토큰화 */
		if(ptr == NULL){
			printf("$ Tokenization Complete\n\n");
			break;
		}
		/* 소괄호, 세미콜론 등을 고려 */
		if (ptr[0] == '('){
			/* 소괄호 START */

			/* 다음 명령어는 Ready_Queue 에 넣을 수 있도록 함 */
			flag = 1;

			/* 일단 소괄호 제거 */
			ptr = &ptr[1];

			/* 설마... */
			if(ptr[strlen(ptr)-1] == ')'){
				flag = 0;
				cmd_list[CMD_LIST_COUNT] = ptr;
				CMD_LIST_COUNT++;
				continue;

			} else if (!strcmp(ptr, ")&")) {
				cmd_list[CMD_LIST_COUNT] = "&&";
				CMD_LIST_COUNT++;
				continue;
			}
			else {
				/* Queue */
				Ready_Queue[Ready_Queue_Count] = ptr;
				Ready_Queue_Count++;
			}
			
		}
		else if(ptr[strlen(ptr)-1] == ')'){
			/* 소괄호 END */

			/* Queue에 있는 내용을 CMD_LIST로 이동 */
			for (i=0; i<Ready_Queue_Count; i++){
				cmd_list[CMD_LIST_COUNT] = Ready_Queue[i];
				CMD_LIST_COUNT++;
			}

			ptr[strlen(ptr)-1] = '\0';

			/* CMD LIST IN */
			cmd_list[CMD_LIST_COUNT] = ptr;
			CMD_LIST_COUNT++;

			/* FLAG 0 */
			flag = 0;
			Ready_Queue_Count = 0;
		}
		else {
			/* 일반적인 명령어 */

			/* FLAG CHECK */
			if (flag){
				/* Ready Queue */
				//printf("$ command to RDQ : %s\n", ptr);
				Ready_Queue[Ready_Queue_Count] = ptr;
				Ready_Queue_Count++;
			}
			else {
				/* Token_Queue */
				//printf("$ command to TKQ : %s\n", ptr);
				Token_Queue[Token_Queue_Count] = ptr;
				Token_Queue_Count++;
			}
		}

		/* tok tok Tropicana */
		ptr = strtok(NULL, " ");
	}

	/* 현재 RQC == 0, TQC != 0 */
	if(Ready_Queue_Count != 0){
		printf("$ Command Tokenization FAILED.\n$ EXIT the SHELL.\n");
		exit(1);
	}

	/* Token Queue -> CMD_LIST */
	for(i=0; i<Token_Queue_Count; i++){
		cmd_list[CMD_LIST_COUNT] = Token_Queue[i];
		CMD_LIST_COUNT++;
	}

	if(CMD_LIST_COUNT > MAX_ARG){
		printf("$ The number of commands that can be input has been exceeded.\n$ EXIT the SHELL.\n");
		exit(1);
	}
	return CMD_LIST_COUNT;
}

/* 연산자 체크 */
int operator_check(char *word){
	printf("[OPCHECK] %s\n", word);
	if(!strcmp(word, "&")) return 1;
	else if(!strcmp(word, ">")) return 2;
	else if(!strcmp(word, ">>")) return 3;
	else if(!strcmp(word, "<")) return 4;
	else if(!strcmp(word, "|")) return 5;
	else if(!strcmp(word, "&&")) return 1;
	else if(!strcmp(word, ";")) return 10;
	else if(!strcmp(word, "~")) return 20;
	else return 0;
}

/* CMD_LIST에 있는 명령어를 꺼내 '올바르게' 실행되도록 하는 함수 */
void command_execute(int cmd_cnt, char *cmd_list[]){
	/* 현재 리스트에는 소괄호에 의한 순서가 적용되어 있음 */

	/* 세미콜론이 나오면 Normal -> Prioirty 으로 이동함 */
	char *Execute_Prior_Queue[MAX_ARG];
	char *Execute_Normal_Queue[MAX_ARG];
	char *temp;
	char *tempname2_string;
	int PQC=0, EQC=0, i, j, k;


	/* 세미콜론 검열 */
	for(i=0; i<cmd_cnt; i++){

		/* semicolon check */
		if(cmd_list[i][strlen(cmd_list[i])-1] == ';'){
			
			/* semicolon */
			for(j=0; j<EQC; j++){
				Execute_Prior_Queue[PQC] = Execute_Normal_Queue[j];
				PQC++;
			}
			EQC = 0;

			/* 세미콜론 제거 후 Prior Queue으로... */
			cmd_list[i][strlen(cmd_list[i])-1] == '\0';
			Execute_Prior_Queue[PQC] = cmd_list[i];
			PQC++;
		}
		else {
			/* not semicolon */
			Execute_Normal_Queue[EQC] = cmd_list[i];
			EQC++;
		}
	}

	char *Operand_Queue1[MAX_ARG/2];
	char *Operand_Queue2[MAX_ARG/2];
	int chk=0, OQC1=0, OQC2=0, bg=0, t=0, ex=0;
	int Remain_Count = EQC + PQC, flag, oldflag;
	
	/* Prior Queue로 합쳐줌 */
	for (k=PQC; k<Remain_Count; k++){
		Execute_Prior_Queue[k] = Execute_Normal_Queue[k-PQC];
	}

	/* 자 명령어들 드가자 */
	while(1){
		flag = operator_check(Execute_Prior_Queue[t]);

		if (flag == 20 && OQC1 == 0) continue;

		/* Semicolon */
		if (flag == 10){
			if(OQC2 != 0){
				shell_execute(oldflag, OQC1, OQC2, Operand_Queue1, Operand_Queue2, bg);
			} else {
				shell_execute(oldflag, OQC1, 0, Operand_Queue1, Operand_Queue1, bg);
			}
			OQC1 = 0; OQC2 = 0; bg = 0;
			Operand_Queue1[OQC1] = Execute_Prior_Queue[t];
		}

		if(chk > 0){
			/* 연산자 입력된 상태 */
			printf("[T] %s\n", Execute_Prior_Queue[t]);
			if(chk > 1 && flag==0){
				Operand_Queue2[OQC2] = Execute_Prior_Queue[t];
				OQC2++;
			}
			else if (flag==0){
				shell_execute(oldflag, OQC1, OQC2, Operand_Queue1, Operand_Queue2, bg);
				OQC1 = 0; OQC2 = 0; bg = 0;
			}
			else {
				shell_execute(oldflag, OQC1, OQC2, Operand_Queue1, Operand_Queue2, bg);
				OQC1 = 0; OQC2 = 0; bg = 0;
			}
		}
		else {
			/* 연산자 입력 안된 상태 */
			if(1 == flag) bg=1;
			else if (1 < flag) {
				chk = 1;
				oldflag = flag;
			} 
			else {
				Operand_Queue1[OQC1] = Execute_Prior_Queue[t];
				OQC1++;
			}
		}
		t++;

		if(t == Remain_Count) break;
	}
	if (OQC1 != 0 || OQC2 != 0){
		if(chk > 1){
			shell_execute(oldflag, OQC1, OQC2, Operand_Queue1, Operand_Queue2, bg);
		}
		shell_execute(oldflag, OQC1, 0, Operand_Queue1, NULL, bg);
	}
}

/* 피연산자 / 연산자 플래그를 입력 받아 연산자에 맞는 함수로 토스
 * command_execute의 역할을 나누어 프로그래밍 부담을 줄였다. 
 */
void shell_execute(int flag, int cnt1, int cnt2, char *list1[], char *list2[], int Background){
	printf("$ [FLAG] %d - shell_EXECUTED!!\n", flag);
	switch (flag){
		case 2:
			input_redirect_owrite(cnt1, cnt2, list1, list2, Background);
			break;
		case 3:
			input_redirect_append(cnt1, cnt2, list1, list2, Background);
			break;
		case 4:
			output_redirect(cnt1, cnt2, list1, list2, Background);
			break;
		case 5:
			single_pipe(cnt1, cnt2, list1, list2, Background);
			break;
		case 6:
			double_pipe(cnt1, cnt2, list1, list2, Background);
			break;
		default:
			break;
	}
}

void input_redirect_owrite(int cnt1, int cnt2, char *list1[], char *list2[], int Background){
	int fd;

	if((fd == open(list2[0], O_RDWR | O_CREAT | S_IROTH | 0644)) < 0){
		printf("$ Redirect FAILED - create file fault\nEXIT the SHELL.\n");
		exit(1);
	}
	dup2(fd, STDOUT_FILENO);
	execvp(list1[0], list1);
	close(fd);
}

void input_redirect_append(int cnt1, int cnt2, char *list1[], char *list2[], int Background){
	int fd;

	if((fd == open(list2[0], O_RDWR | O_APPEND | S_IROTH | 0644)) < 0){
		printf("$ Redirect FAILED - create file fault\nEXIT the SHELL.\n");
		exit(1);
	}
	dup2(fd, STDOUT_FILENO);
	execvp(list1[0], list1);
	close(fd);
}
void output_redirect(int cnt1, int cnt2, char *list1[], char *list2[], int Background){
	int fd, num, i, n;
	char buf[MAX_LINE]; // 파일을 읽어와 내용을 보관할 임시 버퍼
	char *list[MAX_ARG]; // 파일 내용을 토큰화 하고 저장할 버퍼
	pid_t cpid;

	if (fd = open(list2[0], O_RDONLY) < 0) {
		printf("$ 존재하지 않는 파일명 입니다. \n");
		exit(1);
	}
	/* 파일을 표준 입력으로 사용 */
	dup2(fd, STDIN_FILENO);
	close(fd);

	/* 기존 함수 (shell_execute)에서 토큰화 하며 잘린 부분 이어주기 */
	for (i=0; list1[i]!=NULL; i++){
	}
	list1[i] = list2[0];
	list1[i+1] = '\0';

	execvp(list1[0], list1);
	exit(0);
}
void single_pipe(int cnt1, int cnt2, char *list1[], char *list2[], int Background){
	int i, tmp_fd;
	pid_t cpid1, cpid2;
	int fd[2];
	char buf[MAX_LINE];
	pipe(fd);

	cpid1 = fork();
	if (cpid1 < 0) {
		printf("$ 프로세스 생성 오류 \n");
		exit(1);
	} 
	else if (cpid1 == 0) {
		close(fd[0]);
		dup2(fd[1], STDOUT_FILENO);
		close(fd[1]);
		execvp(list1[0], list1);
		exit(0);
	} 
	else {
		cpid2 = fork();
		if (cpid2 < 0) {
			printf("$ 프로세스 생성 오류\n");
		}
		else if (cpid2 == 0) {
			close(fd[1]);
			dup2(fd[0], STDIN_FILENO);
			close(fd[0]);
			execvp(list2[0], list2);	
			exit(0);
		} else {
			wait(NULL);
			exit(0);
		}
	}
	exit(0);
}
void double_pipe(int cnt1, int cnt2, char *list1[], char *list2[], int Background);

/*
앰퍼센트는 바로 앞의 명령을 백그라운드로 실행합니다.
소괄호 뒤라면 소괄호 안의 내용을 순차적으로 백그라운드 실행합니다. 
앰퍼센트는 문장 중간에도 올 수 있습니다
소괄호는 띄어쓰기 없이 들어갑니다

명령과 세미콜론 사이에는 띄어쓰기가 들어가지 않습니다.
백그라운드 기호(&)역시 마찬가지로 띄어쓰기 없이 사용합니다.

괄호는 백그라운드 처리를 위해 사용하는 것이 아니라, 명령어 그룹을 표현하기 위해 사용합니다.
괄호 안에 있는 내용들이 순차적으로 처리되어야 합니다. 
*/