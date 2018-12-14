#include <stdio.h>
#include <iostream>
#include <string>
#include <cstring>
#include <sstream>
#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <glob.h>
#include <vector>
#include <sys/times.h>
#include <iomanip>
#include <signal.h>

using namespace std;
enum types_of_input_output_changes: short {no_change, input_changed, output_changed, output_and_input_changed, error};
typedef enum types_of_input_output_changes types_of_input_output_changes;
struct changes_in_input_output_streams{
    types_of_input_output_changes type;
    string command;
    string new_output = "";
    string new_input = "";
};
struct result_times{
    double real;
    double user;
    double sys;
};
typedef struct result_times result_times;
typedef struct changes_in_input_output_streams changes_in_input_output_streams;
struct arguments{
    char ** pointer = NULL;
    int size = 0;
};
typedef struct arguments arguments;
int count_number_of_symbols(string s, char c){
    int i = 0;
    int count = 0;
    for(i = 0; i < s.size(); i++){
        if(s[i] == c){
            count++;
        }
    }
    return count;
}
changes_in_input_output_streams parse_the_line_to_find_changes_of_input_output_streams(string s){
    changes_in_input_output_streams result;
    if(count_number_of_symbols(s, '>') <= 1 && count_number_of_symbols(s, '<') <= 1) {
        int first_output_simbol = s.find('>');
        int first_input_simbol = s.find('<');
        if (first_input_simbol == -1 && first_output_simbol == -1) {
            result.type = no_change;
            result.command = s;
        } else if (first_input_simbol == -1 && first_output_simbol != -1) {
            if(first_output_simbol + 2 < s.size()) {//проверка на то, что есть файл, куда перенаправлять
                result.type = output_changed;
                result.command = s.substr(0, first_output_simbol - 1);
                result.new_output = s.substr(first_output_simbol + 2);
            }
            else{
                result.type = error;
                result.command = s;
                result.new_output = "";
                result.new_input = "";
            }

        } else if (first_input_simbol != -1 && first_output_simbol == -1) {
            if(first_input_simbol + 2 < s.size()) {//проверка на то, что есть файл, куда перенаправлять
                result.type = input_changed;
                result.command = s.substr(0, first_input_simbol - 1);
                result.new_input = s.substr(first_input_simbol + 2);
            }
            else{
                result.type = error;
                result.command = s;
                result.new_output = "";
                result.new_input = "";
            }
        } else {
            result.type = output_and_input_changed;
            if (first_input_simbol < first_output_simbol) {
                if(first_output_simbol + 2 < s.size() &&  first_output_simbol - first_input_simbol > 0) {//проверка на то, что есть оба файла, куда перенаправлять
                    result.command = s.substr(0, first_input_simbol - 1);
                    result.new_input = s.substr(first_input_simbol + 2, first_output_simbol - first_input_simbol - 3);
                    result.new_output = s.substr(first_output_simbol + 2);
                }
                else{
                    result.type = error;
                    result.command = s;
                    result.new_output = "";
                    result.new_input = "";
                }
            } else if (first_input_simbol > first_output_simbol) {
                if(first_input_simbol + 2 < s.size() && first_input_simbol - first_output_simbol > 0) {//проверка на то, что есть оба файла, куда перенаправлять
                    result.command = s.substr(0, first_output_simbol - 1);
                    result.new_output = s.substr(first_output_simbol + 2, first_input_simbol - first_output_simbol - 3);
                    result.new_input = s.substr(first_input_simbol + 2);
                }
                else{
                    result.type = error;
                    result.command = s;
                    result.new_output = "";
                    result.new_input = "";
                }
            }
        }
    }
    else{
        result.type = error;
        result.command = s;
    }
    return result;
}

int changedir(string s){
    int a = chdir(s.c_str());
    return a;
}

int count_number_of_words(string s){
    int i = 0;
    int count = 1;
    for(i = 0; i < s.size(); i++){
        if(s[i] == ' '){
            count++;
        }
    }
    return count;
}
int count_number_of_commands(string s){
    int i = 0;
    int count = 1;
    for(i = 0; i < s.size(); i++){
        if(s[i] == '|'){
            count++;
        }
    }
    return count;
}
vector<string> get_arguments(string command_line){
    istringstream command_stream(command_line);
                string command;
                vector<string> vector_of_arguments;
                while(command_stream >> command){
                    if(command.find_first_of("*?") == -1) {
                        vector_of_arguments.push_back(command);
                    }
                    else{
                        glob_t glob_struct;
                        glob(command.c_str(), 0, NULL, &glob_struct);
                        if(glob_struct.gl_pathc != 0) {//проверка на то, что существуют имена файлов, соответствующие регулярному выражению
                            for (int i = 0; i < glob_struct.gl_pathc; i++) {
                                vector_of_arguments.push_back(string(*(glob_struct.gl_pathv + i)));
                            }
                        }
                        else{
                            vector_of_arguments.push_back(command);
                        }
                    }

             }
    return vector_of_arguments;
}
int pwd(){
    char dir[FILENAME_MAX];
    getcwd(dir, FILENAME_MAX);
    cout << dir << "\n";
    return 0;
}
void signal_processing_for_child(int signal_number){
    kill(getpid(),SIGKILL);
}
int main() {
    string all_input;
    string command_word;
    string s;
    int i = 0;
    do {
        char invite[2];
        invite[0] = '!';
        invite[1] = '>';
        char dir[100];
        getcwd(dir, 100);
        if (getuid()) {
            cout << dir << " " << invite[1];
        } else {
            cout << dir << " " << invite[0];
        }
        signal(SIGINT, SIG_IGN);
        char x = getchar();
        if(x != EOF && x != '\n'){//проверка на ctrl + D и пустой ввод
            getline(cin, all_input);
            all_input.insert(all_input.begin(), x);//возвращаем в начало строки прочитанный x, если это не EOF и не \n
            istringstream all_input_stream(all_input);
            int number_of_commands_in_conveyor = count_number_of_commands(all_input);
            int array_of_file_descriptors[number_of_commands_in_conveyor - 1][2];
            for (int j = 0; j < number_of_commands_in_conveyor; j++) {
                getline(all_input_stream, s, '|');
                if(j != number_of_commands_in_conveyor - 1) {
                    pipe(array_of_file_descriptors[j]);
                }
                istringstream command_word_stream(s);
                command_word_stream >> command_word;
                if (command_word != "exit" && command_word != "cd" && command_word != "pwd" && command_word != "time") {
                    pid_t pid = fork();
                    if (pid == 0) {
                        signal(SIGINT, signal_processing_for_child);
                        int stdin_copy = dup(0);
                        int stdout_copy = dup(1);
                        if (j == 0 && number_of_commands_in_conveyor != 1) {
                            dup2(array_of_file_descriptors[0][1], 1);
                            close(array_of_file_descriptors[0][0]);
                            for(int temp = 1; temp < number_of_commands_in_conveyor - 1; temp++){//закрываем все лишние дескрипторы для самого первого процесса в конвеере
                                close(array_of_file_descriptors[temp][0]);
                                close(array_of_file_descriptors[temp][1]);
                            }
                        } else if (j == number_of_commands_in_conveyor - 1) {
                            dup2(array_of_file_descriptors[number_of_commands_in_conveyor - 2][0], 0);
                            close(array_of_file_descriptors[number_of_commands_in_conveyor - 2][1]);
                            for(int temp = 0; temp < number_of_commands_in_conveyor - 2; temp++){//закрываем все лишние дескрипторы для самого последнего процесса в конвеере
                                close(array_of_file_descriptors[temp][0]);
                                close(array_of_file_descriptors[temp][1]);
                            }
                        } else if(j != 0 && j != number_of_commands_in_conveyor - 1){
                            dup2(array_of_file_descriptors[j - 1][0], 0);
                            dup2(array_of_file_descriptors[j][1], 1);
                            close(array_of_file_descriptors[j - 1][1]);
                            close(array_of_file_descriptors[j][0]);
                            for(int temp = 0; temp < j - 1; temp++){//закрываем все лишние дескрипторы для всех процессов посередине конвеера
                                close(array_of_file_descriptors[temp][0]);
                                close(array_of_file_descriptors[temp][1]);
                            }
                            for(int temp = j + 1; temp < number_of_commands_in_conveyor - 1; temp++){
                                close(array_of_file_descriptors[temp][0]);
                                close(array_of_file_descriptors[temp][1]);
                            }
                        }
                        changes_in_input_output_streams changes = parse_the_line_to_find_changes_of_input_output_streams(s);//перенаправление ввода-вывода
                        int fd_1;
                        int fd_2;
                        if (changes.type == input_changed) {
                            if(j == 0) {
                                fd_1 = open((changes.new_input).c_str(), O_RDONLY);
                                if (fd_1 >= 0) {
                                    dup2(fd_1, 0);
                                } else {
                                    perror((changes.new_input).c_str());
                                    exit(0);
                                }
                            }
                            else{
                                perror("wrong command in conveyor");//попытка перенаправить ввод у не первого процесса конвеера, если только там не одна команда
                                exit(0);
                            }
                        } else if (changes.type == output_changed) {
                            if(j == number_of_commands_in_conveyor - 1) {
                                fd_2 = open((changes.new_output).c_str(), O_CREAT | O_TRUNC | O_RDWR, 0666);
                                dup2(fd_2, 1);
                            }
                            else{
                                perror("wrong command in conveyor");//попытка перенаправить вывод в файл у не последнего процесса конвеера, если только там не одна команда
                                exit(0);
                            }
                        } else if (changes.type == output_and_input_changed) {
                            if(number_of_commands_in_conveyor == 1) {
                                fd_1 = open((changes.new_input).c_str(), O_RDONLY);
                                if (fd_1 >= 0) {
                                    dup2(fd_1, 0);
                                } else {
                                    perror((changes.new_input).c_str());
                                    exit(0);
                                }
                                fd_2 = open((changes.new_output).c_str(), O_CREAT | O_TRUNC | O_RDWR, 0666);
                                dup2(fd_2, 1);
                            }
                            else{
                                perror("wrong command in conveyor");//попытка перенаправить и ввод, и вывод в конвеере не из одного элемента
                                exit(0);
                            }
                        }

                        else if(changes.type == error){
                            perror(s.c_str());
                            exit(0);
                        }
                        vector<string> vector_of_arguments = get_arguments(changes.command);//обработка регулярных выражений и создание вектора аргументов
                        char *ready_function_arguments[vector_of_arguments.size() + 1];
                        for (int i = 0; i < vector_of_arguments.size(); i++) {//перевод вектора в массив
                            ready_function_arguments[i] = (char *) malloc(
                                    (vector_of_arguments[i].size() + 1) * sizeof(char));
                            strcpy(ready_function_arguments[i], (vector_of_arguments[i]).c_str());
                        }
                        ready_function_arguments[vector_of_arguments.size()] = 0;
                        char **const pointer = &(ready_function_arguments[0]);
                        //                for(int i = 0; i < vector_of_arguments.size(); i++){
                        //                    cout << "from_ready_args" << (*(pointer + i)) << "\n";
                        //                }
                        execvp(*(pointer), pointer);
                        perror("execvp");
                        for (int i = 0; i < vector_of_arguments.size(); i++) {//очистка памяти в случае неудачи execvp
                            free(*(pointer + i));
                        }
                        exit(0);
                    } else if (pid > 0) {
                        if (j == 0 && number_of_commands_in_conveyor != 1) {//закрытие дескрипторов в родительском процессе. Закрываем только те, которые уже назначены дочерним процессам
                            close(array_of_file_descriptors[0][1]);
                        } else if (j == number_of_commands_in_conveyor - 1) {
                            close(array_of_file_descriptors[number_of_commands_in_conveyor - 2][0]);
                        } else if(j != 0 && j != number_of_commands_in_conveyor - 1){
                            close(array_of_file_descriptors[j - 1][0]);
                            close(array_of_file_descriptors[j][1]);
                        }
                    }
                    else{
                        perror("fork failed");
                    }
                } else if (command_word == "cd") {
                    string directory;
                    command_word_stream >> directory;
                    changedir(directory);
                } else if (command_word == "pwd") {
                    int stdout_copy = dup(1);
                    if(j != number_of_commands_in_conveyor - 1) {//если pwd в конвеере
                        dup2(array_of_file_descriptors[j][1], 1);
                    }
                    changes_in_input_output_streams changes = parse_the_line_to_find_changes_of_input_output_streams(s);// если было перенаправление ввода-вывода
                    if (changes.type == output_changed) {
                        if(j == number_of_commands_in_conveyor - 1) {
                            int fd = open((changes.new_output).c_str(), O_CREAT | O_TRUNC | O_RDWR, 0666);
                            dup2(fd, 1);
                            pwd();
                            close(fd);
                            if (j == 0 && number_of_commands_in_conveyor != 1) {//мы не уходили из родительского процесса, поэтому надо закрыть ненужные дескрипторы для соответствующего канала
                                close(array_of_file_descriptors[0][1]);
                            } else if (j == number_of_commands_in_conveyor - 1) {
                                close(array_of_file_descriptors[number_of_commands_in_conveyor - 2][0]);
                            } else if (j != 0 && j != number_of_commands_in_conveyor - 1) {
                                close(array_of_file_descriptors[j - 1][0]);
                                close(array_of_file_descriptors[j][1]);
                            }
                            dup2(stdout_copy, 1);
                            close(stdout_copy);
                        }
                        else{
                            perror("wrong command in conveyor");
                            exit(0);
                        }
                    } else if (changes.type == no_change) {
                        pwd();
                        if (j == 0 && number_of_commands_in_conveyor != 1) {
                            close(array_of_file_descriptors[0][1]);
                        } else if (j == number_of_commands_in_conveyor - 1) {
                            close(array_of_file_descriptors[number_of_commands_in_conveyor - 2][0]);
                        } else if(j != 0 && j != number_of_commands_in_conveyor - 1){
                            close(array_of_file_descriptors[j - 1][0]);
                            close(array_of_file_descriptors[j][1]);
                        }
                        dup2(stdout_copy, 1);
                        close(stdout_copy);
                    }
                    else if(changes.type == error){
                        perror(s.c_str());
                        exit(0);
                    }
                    else{
                        perror("wrong command");//попытка перенаправить ввод или и ввод, и вывод
                        exit(0);
                    }
                } else if (command_word == "time") {
                    signal(SIGINT, SIG_IGN);
                    struct tms *start_times = (struct tms *) malloc(sizeof(struct tms));
                    struct tms *finish_times = (struct tms *) malloc(sizeof(struct tms));
                    clock_t start = times(start_times);
                    pid_t pid = fork();
                    if (pid == 0) {
                        signal(SIGINT, signal_processing_for_child);
                        changes_in_input_output_streams changes = parse_the_line_to_find_changes_of_input_output_streams(s);
                        if (j == 0 && number_of_commands_in_conveyor != 1) {
                            dup2(array_of_file_descriptors[0][1], 1);
                            close(array_of_file_descriptors[0][0]);
                            for(int temp = 1; temp < number_of_commands_in_conveyor - 1; temp++){
                                close(array_of_file_descriptors[temp][0]);
                                close(array_of_file_descriptors[temp][1]);
                            }
                        } else if (j == number_of_commands_in_conveyor - 1) {
                            dup2(array_of_file_descriptors[number_of_commands_in_conveyor - 2][0], 0);
                            close(array_of_file_descriptors[number_of_commands_in_conveyor - 2][1]);
                            for(int temp = 0; temp < number_of_commands_in_conveyor - 2; temp++){
                                close(array_of_file_descriptors[temp][0]);
                                close(array_of_file_descriptors[temp][1]);
                            }
                        } else if(j != 0 && j != number_of_commands_in_conveyor - 1){
                            dup2(array_of_file_descriptors[j - 1][0], 0);
                            dup2(array_of_file_descriptors[j][1], 1);
                            close(array_of_file_descriptors[j - 1][1]);
                            close(array_of_file_descriptors[j][0]);
                            for(int temp = 0; temp < j - 1; temp++){
                                close(array_of_file_descriptors[temp][0]);
                                close(array_of_file_descriptors[temp][1]);
                            }
                            for(int temp = j + 1; temp < number_of_commands_in_conveyor - 1; temp++){
                                close(array_of_file_descriptors[temp][0]);
                                close(array_of_file_descriptors[temp][1]);
                            }
                        }
                        int fd_1;
                        int fd_2;
                        if (changes.type == input_changed) {
                            if(j == 0) {
                                fd_1 = open((changes.new_input).c_str(), O_RDONLY);
                                if (fd_1 >= 0) {
                                    dup2(fd_1, 0);
                                } else {
                                    perror((changes.new_input).c_str());
                                    exit(0);
                                }
                            }
                            else{
                                perror("wrong command in conveyor");
                                exit(0);
                            }
                        } else if (changes.type == output_changed) {
                            if(j == number_of_commands_in_conveyor - 1) {
                                fd_2 = open((changes.new_output).c_str(), O_CREAT | O_TRUNC | O_RDWR, 0666);
                                dup2(fd_2, 1);
                            }
                            else{
                                perror("wrong command in conveyor");
                                exit(0);
                            }
                        } else if (changes.type == output_and_input_changed) {
                            if(number_of_commands_in_conveyor == 1) {
                                fd_1 = open((changes.new_input).c_str(), O_RDONLY);
                                if (fd_1 >= 0) {
                                    dup2(fd_1, 0);
                                } else {
                                    perror((changes.new_input).c_str());
                                    exit(0);
                                }
                                fd_2 = open((changes.new_output).c_str(), O_CREAT | O_TRUNC | O_RDWR, 0666);
                                dup2(fd_2, 1);
                            }
                            else if(changes.type == error){
                                perror("wrong command in conveyor");
                                exit(0);
                            }
                        }
                        else if(changes.type == error){
                            perror(s.c_str());
                            exit(0);
                        }
                        vector<string> vector_of_arguments = get_arguments(changes.command);
                        char *ready_function_arguments[vector_of_arguments.size()];
                        for (int i = 0; i < vector_of_arguments.size() - 1; i++) {
                            ready_function_arguments[i] = (char *) malloc(
                                    (vector_of_arguments[i + 1].size() + 1) * sizeof(char));
                            strcpy(ready_function_arguments[i], (vector_of_arguments[i + 1]).c_str());
                        }
                        ready_function_arguments[vector_of_arguments.size() - 1] = 0;
                        char **const pointer = &(ready_function_arguments[0]);
                        execvp(*(pointer), pointer);
                        perror("execvp");
                        for (int i = 0; i < vector_of_arguments.size(); i++) {
                            free(*(pointer + i));
                        }
                        exit(0);
                    } else if (pid > 0) {
                        signal(SIGINT, SIG_IGN);
                        if (j == 0 && number_of_commands_in_conveyor != 1) {
                            close(array_of_file_descriptors[0][1]);
                        } else if (j == number_of_commands_in_conveyor - 1) {
                            close(array_of_file_descriptors[number_of_commands_in_conveyor - 2][0]);
                        } else if(j != 0 && j != number_of_commands_in_conveyor - 1){
                            close(array_of_file_descriptors[j - 1][0]);
                            close(array_of_file_descriptors[j][1]);
                        }
                        wait(0);
                        clock_t finish = times(finish_times);
                        result_times times;
                        times.real = ((double) (finish - start)) / CLOCKS_PER_SEC;
                        times.user =
                                ((double) (finish_times->tms_cutime - start_times->tms_cutime)) / sysconf(_SC_CLK_TCK);
                        times.sys =
                                ((double) (finish_times->tms_cstime - start_times->tms_cstime)) / sysconf(_SC_CLK_TCK);
                        cerr << "real:\n" << fixed << setprecision(6) << times.real << "\n";
                        cerr << "user:\n" << times.user << "\n";
                        cerr << "sys:\n" << times.sys << "\n";
                        free(start_times);
                        free(finish_times);
                    }
                    else{
                        perror("fork failed");
                    }
                }
            }
            int status;
            while (wait(&status) > 0) {
            }
        }
        else if(x == EOF){
            return 0;
        }
    }while (s != "exit");
}