#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#define MAX_ARG 8
#define MAX_LINE 80

int shell_inputScan(char prompt[], char *cmd_list[]);
void shell_execute(int num, char *cmd_list[]);
int *shell_flag(int num, char *cmd_list[]);
void shell_input_redirect(char *cmd1[], char *cmd2[]);
void shell_output_redirect(char *cmd1[], char *cmd2[]);
void shell_pipe(char *cmd1[], char *cmd2[]);

int main(){
	char prompt[MAX_LINE];
	char buf[MAX_LINE];
	char *cmd_list[MAX_ARG];
	char *exitstr = "exit";
	char *backstr = "&";
	int num, status;
	pid_t pid;

	while(1){
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

			/* 입력되는 문장을 받아 inputScan 실행 */
			memset(prompt, 0, sizeof(char) * MAX_LINE); // 프롬프트 버퍼 초기화
			if (!fgets(prompt, MAX_LINE, stdin)) break;
			fflush(stdin); // 표준 입력 스트림 버퍼 초기화

			/* inputScan 실행 결과에 따라 execute 실행 또는 쉘 종료 */
			if ((num = shell_inputScan(prompt, cmd_list)) < 1) {
				if (num == 0) {
					printf("Custom Shell $ 잘못된 입력입니다.\n");
				} 
				else {
					printf("Custom Shell $ 쉘을 종료합니다. \n");
					write(exit_pipe[1], exitstr, strlen(exitstr)); // 파이프를 통해 EXIT 작성
					close(exit_pipe[1]); // 파이프 WRITE 닫음
				}
				exit(0);
			}

			/* 백그라운드 명령어 확인 */
			if(!strcmp(cmd_list[num-1], backstr)){
				write(exit_pipe[1], backstr, strlen(backstr));
				cmd_list[num-1] = '\0';
				num-=1;
			}
			close(exit_pipe[1]); // 파이프 WRITE 닫음
			shell_execute(num, cmd_list);
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
			if (!strcmp(buf, exitstr)) exit(0);

			/* Background 옵션이 확인되면 wait 변경 */
			if (buf[0] == backstr[0]) {
				printf("[1] %d\n", getpid());
				waitpid(pid, &status, WNOHANG);
			} else {
				wait(&status);
			}
		}
	}
	return 0;
}


/*
   shell_flag()
   입력 받은 문자 스트림에 <, >, | 가 포함되었는지 알려주는 함수
   정수 배열 (플래그, 위치) 리턴.
   플래그>> 없음 : 0, > : 1, < : 2, | : 3. 
*/
int *shell_flag(int num, char*cmd_list[]){
	int i;
	static int rtn[2] = {0, 0};

	for (i=0; i<num; i++) {
		if (!strcmp(cmd_list[i], ">")) {
			rtn[0] = 1;
			rtn[1] = i;
		}
		else if (!strcmp(cmd_list[i], "<")) {
			rtn[0] = 2;
			rtn[1] = i;
		}
		else if (!strcmp(cmd_list[i], "|")) {
			rtn[0] = 3;
			rtn[1] = i;
		}
	}
	return rtn;
}


/* 
   shell_execute()
   입력 : 리스트 사이즈, 리스트
   커맨드 리스트에서 명령어들을 구분하고, 명령어를 실행
*/
void shell_execute(int num, char *cmd_list1[]){
	char *cmd_list2[MAX_ARG];

	/* 
	   입력된 명령어에 따라 flag를 설정하고, flag에 맞는 명령어 실행
	*/
	int i;
	int *flags = shell_flag(num, cmd_list1);

	/* 리다이렉션이나 파이프가 있는 경우,
	   cmd_list[]를 잘라서 cmd_list2를 생성
	   정확히는 cmd_list 마지막에 NULL에 입력하고 cmd_list2으로 뒷부분 포인팅
	   ls -al | sort [cmd_list1 = {ls, -al, |, sort}, flags[1] = 2];
	   */
	if (flags[0] != 0){
		for (i=flags[1]+1; i<num; i++) {
			cmd_list2[i-flags[1]-1] = cmd_list1[i];
		}
		cmd_list2[num-flags[1]-1] = NULL;
		cmd_list1[flags[1]] = NULL;
	}

	switch (flags[0]) {
		case 0: /* 단일 명령어 실행인 경우 */
			execvp(cmd_list1[0], cmd_list1);
			exit(0);
		case 1: /* input_redirection > */
			shell_input_redirect(cmd_list1, cmd_list2);
			exit(0);
		case 2: /* output_redirection < */
			shell_output_redirect(cmd_list1, cmd_list2);
			exit(0);
		case 3: /* pipe */
			shell_pipe(cmd_list1, cmd_list2);
			exit(0);
		default:
			printf("& 실행 오류 !\n");
			exit(1);
	}
}

/* 명령어 + 옵션 > 파일명 */
void shell_input_redirect(char *cmd1[], char *cmd2[]){
	int fd;

	if ((fd = open(cmd2[0], O_RDWR | O_CREAT | S_IROTH, 0644)) < 0) {
		printf("$ 파일 생성 오류\n");
		exit(1);
	}
	dup2(fd, STDOUT_FILENO);
	execvp(cmd1[0], cmd1);
	close(fd);
	exit(0);
}

/* 명령어 + 옵션 < 파일명 
   파일을 읽고, 읽은 내용을 명령어에 전달
   파라미터 : cmd1은 명령어와 옵션, cmd2는 파일명
   */
void shell_output_redirect(char *cmd1[], char *cmd2[]){
	int fd, num, i, n;
	char buf[MAX_LINE]; // 파일을 읽어와 내용을 보관할 임시 버퍼
	char *list[MAX_ARG]; // 파일 내용을 토큰화 하고 저장할 버퍼
	pid_t cpid;

	if (fd = open(cmd2[0], O_RDONLY) < 0) {
		printf("$ 존재하지 않는 파일명 입니다. \n");
		exit(1);
	}
	/* 파일을 표준 입력으로 사용 */
	dup2(fd, STDIN_FILENO);
	close(fd);

	/* 기존 함수 (shell_execute)에서 토큰화 하며 잘린 부분 이어주기 */
	for (i=0; cmd1[i]!=NULL; i++){
	}
	cmd1[i] = cmd2[0];
	cmd1[i+1] = '\0';

	execvp(cmd1[0], cmd1);
	exit(0);
}

/* 명령어 + 옵션 | 명령어 + 옵션
   자식 프로세스 (손자 프로세스)를 이용하여 
   명령어1의 출력을 받아 명령어2의 입력으로 넣어준다. */
void shell_pipe(char *cmd1[], char *cmd2[]){
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
		execvp(cmd1[0], cmd1);
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
			execvp(cmd2[0], cmd2);	
			exit(0);
		} else {
			wait(NULL);
			exit(0);
		}
	}
	exit(0);
}


/*
   shell_inputScan()
   사용자의 입력이 기록되어 있는 prompt에서 명령어(단어)들을 토큰화 하는 함수
   입력 규격에 대해서는 최대 인자 개수만 체크
   토큰 개수를 리턴, exit의 경우 -1를 리턴하며 규격 오류는 0 리턴 
*/
int shell_inputScan(char prompt[], char *cmd_list[]){
	/* 입력 스트림에 있는 \n을 잘라냄 */
	if(prompt[strlen(prompt)-1] == '\n') prompt[strlen(prompt)-1] = '\0';

	/* EXIT 입력을 받은 경우 */
	if (!strcmp(prompt, "exit")) {
		cmd_list[0] = prompt;
		return -1;
	}

	int i=0;
	char *ptr = strtok(prompt, " ");

	while(ptr != NULL){
		if (i+1 > MAX_ARG) return 0; /* 최대 인자 초과 */
		cmd_list[i] = ptr;
		i++;
		ptr = strtok(NULL, " ");
	}
	return i;
}


