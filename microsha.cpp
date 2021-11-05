#include <sys/types.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <iostream>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <limits.h>
#include <glob.h>

std::string getDir(){
	char buff[PATH_MAX];
	getcwd(buff, PATH_MAX);
	return std::string(buff);
}
struct NodeDesc {
	size_t desc;
	std::string file;
	char rwx;
};

struct rDesc {
	int desc1;
	int desc2;
};

struct NodeProg {
	std::vector<NodeDesc> descs;
	std::string progName;
	std::vector<std::string> argv;
};

struct Command {
	std::vector<NodeProg> progs;
};


int getinput(std::string& input) {
	const size_t buf_size = 1024; //
	
	char* buf = (char*)malloc(buf_size * sizeof(char));
	size_t str_cnt = 1;
	buf[str_cnt - 1] = 'a';
	while (str_cnt && !(buf[str_cnt - 1] == '\n' || buf[str_cnt -1 ] == '\0')) {
		str_cnt = read(0, buf, 1024);
		if(str_cnt != 0){
			input.assign(buf, str_cnt);
		}/*else{
			write(2, "EOF", 4);
		}
		*/
	}
	free(buf);
	return input.size();
}

/*char getinput(std::string& input) {
	std::cin >> input;
	return 0;
}*/
char parceargs(const std::string& input, size_t i, Command& cmd) {
	cmd.progs.clear();
	return 0;
}

bool isskipablesymb(char c) {
	return c == ' ';
}

void skip(const std::string& input, size_t& i) {
	while (isskipablesymb(input[i])) {
		i++;
	}
}

const char specialSymb[] = {'<', '|', '>', '&', '$', '\0', '\n'};

bool isspecialsymb(char c) {
	bool ret = false;
	for (auto e : specialSymb) {
		ret |= (e == c);
	}
	return ret;
}

size_t parcefile(const std::string& input, size_t& i, std::string& fileName) {
	size_t st_i = i;
	fileName.clear();
	skip(input, i);
	while (!(isskipablesymb(input[i]) || isspecialsymb(input[i]))) {
		fileName += input[i];
		i++;
	}
	skip(input, i);
	return fileName.size();
}

bool isnumber(std::string& str) {
	if (str == "") return 0;
	bool ret = true;
	for (auto e : str) {
		if (!('0' <= e && e <= '9')) 
			ret = false;
	}
	return ret;
}
size_t parceargs(const std::string& input, size_t& i, std::vector<std::string>& argv, std::vector<NodeDesc>& descs) {
	size_t st_i = i;
	skip(input, i);
	std::string arg;
	while (!(isskipablesymb(input[i]) || isspecialsymb(input[i]))) {
		arg += input[i];
		i++;
	}
	if (input[i] == '<' || input[i] == '>') {
		char** p;
		size_t descriptor = 0;
		char rwx = 'r';
		if (input[i] == '>'){
			descriptor = 1;
			rwx = 'w';
		}
			
		if (isnumber(arg)) {
			descriptor = atoll(arg.c_str());
		}
		i++;
		std::string file;
		if (!parcefile(input, i, file)) {
			return 0;
		}
		skip(input, i);
		descs.push_back({ descriptor, file, rwx });
		return arg.size() + 1 + file.size();
	}
	if(arg.size()) argv.push_back(arg);
	skip(input, i);
	return arg.size();
}

size_t parceprog(const std::string& input, size_t& i, NodeProg& prog) {
	size_t st_i = i;
	parcefile(input, i, prog.progName);
	prog.argv.push_back(prog.progName);
	while (parceargs(input, i, prog.argv, prog.descs));
	return prog.progName.size();
}
size_t parcetunel(const std::string& input, size_t& i, Command& cmd) {
	NodeProg prog;
	size_t rdcl = parceprog(input, i, prog);
	if (rdcl) {
		cmd.progs.push_back(prog);
	}
	skip(input, i);
	if (input[i] == '|') {
		i++;
		if (!rdcl) {
			return 0;
		}
		skip(input, i);
		size_t rdcr = parcetunel(input, i, cmd);
		if (!rdcr) {
			return 0;
		}
		skip(input, i);
		return rdcl + 1;
	}
	return rdcl;
}

char parcecommand(const std::string& input, Command& cmd) {
	cmd.progs.clear();
	size_t i = 0;
	parcetunel(input,i, cmd);
	return 0;
}

void doGlob(const std::string& argve, std::vector<char*>& args){
	glob_t glob_result;
	glob(argve.c_str(), GLOB_TILDE, NULL, &glob_result);
	glob_result.gl_pathv;
	for(int i = 0; i < glob_result.gl_pathc; i++){
		args.push_back((char*)glob_result.gl_pathv[i]);
	}
	//Если поиск по маске ничего не дал, то воспринимаем ее как аргумент
	if(!glob_result.gl_pathc) args.push_back((char*)argve.c_str());
	printf("Glob result:\n");
	for(auto& e: args){
		printf("%s\n", e);
	}
	printf("------------------\n\n");
}

char executecmd(const Command& cmd){
	int fd[2][2];
	fd[0][0] = 0;
	fd[0][1] = 1;
	fd[1][0] = 0;
	fd[1][1] = 1;
	for(int i = 0; i < cmd.progs.size(); i++){
		
		//pipe(fd[0]);
		fd[0][0] = fd[1][0];
		fd[0][1] = fd[1][1];
		if(i != cmd.progs.size() - 1){
			pipe(fd[1]);
		}
		std::vector<char*> args;
		for(auto& argve: cmd.progs[i].argv){
			//Проверяем, нужно ли заменить маску на массив аргументов
			bool to_globing = false;
			for(auto e: argve){
				if(e == '?' || e == '*') to_globing = true;
			}
			if (to_globing){
				doGlob(argve, args);
			}
			else{
				args.push_back((char*)argve.c_str());
			}
			
			
		}

		pid_t pid = fork();
		if (pid == 0) {
			if (i) {
				close(fd[0][1]);
				dup2(fd[0][0],0);
			}
			if (i != cmd.progs.size() - 1){
				close(fd[1][0]);
				dup2(fd[1][1],1);
			}
			for(auto& e: cmd.progs[i].descs){
				if(e.rwx == 'r'){
					int desc = open((char*)e.file.c_str(), O_RDONLY);
					if(desc < 0){
						write(2, "Ошибка открытия файла", 41);
					}else{
						dup2(desc, e.desc);
					}
					
				} else if(e.rwx == 'w'){
					int desc = open((char*)e.file.c_str(), O_WRONLY | O_CREAT, S_IWRITE | S_IREAD);
					if(desc < 0){
						write(2, "Ошибка открытия файла", 41);
					}else{
						dup2(desc, e.desc);
					}
				}
			}
			args.push_back(nullptr);
			if (cmd.progs[i].progName == "pwd"){
				printf("%s\n", getDir().c_str());
			}
			if(!(cmd.progs[i].progName == "pwd" || cmd.progs[i].progName == "cd")){
				execvp(args[0], &args[0]);
			}
			return -1;
		}
		if (cmd.progs[i].progName == "cd"){
			chdir(args[1]);
		}
		if (i) {
			close(fd[0][0]);
		}
		if (i != cmd.progs.size() - 1){
			close(fd[1][1]);
		}
	}
	for(int i = 0; i < cmd.progs.size(); i++){
		int status;
		wait(&status);
	}
	printf("\n");
	return 0;
}

char startlistening(Command& cmd) {
	char ret = 1;
	while(ret){
		std::string input;
		printf("%s:", getDir().c_str());
		fflush(stdout);
		ret = getinput(input);
		if(ret){
			parcecommand(input, cmd);
			if (executecmd(cmd) == -1){
				exit(0);
			}
		}
		
	}
	return 0;
}


int main() {
	Command cmd;
	startlistening(cmd);
	return 0;
}