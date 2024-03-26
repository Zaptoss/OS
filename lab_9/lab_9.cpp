#include <iostream>
#include <string.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <signal.h>

#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <vector>

#include <semaphore.h>
#include <pthread.h>

using namespace std;

const string verse = "Любіть Україну, як сонце, любіть,\n"
"як вітер, і трави, і води...\n"
"В годину щасливу і в радості мить,\n"
"любіть у годину негоди.\n"
"Любіть Україну у сні й наяву,\n"
"вишневу свою Україну,\n"
"красу її, вічно живу і нову,\n"
"і мову її солов’їну.\n"
"Між братніх народів, мов садом рясним,\n"
"сіяє вона над віками…\n"
"Любіть Україну всім серцем своїм\n"
"і всіми своїми ділами.\n"
"Для нас вона в світі єдина, одна\n"
"в просторів солодкому чарі...\n"
"Вона у зірках, і у вербах вона,\n"
"і в кожному серця ударі,\n"
"у квітці, в пташині, в електровогнях,\n"
"у пісні у кожній, у думі,\n"
"в дитячий усмішці, в дівочих очах\n"
"і в стягів багряному шумі…\n"
"Як та купина, що горить — не згора,\n"
"живе у стежках, у дібровах,\n"
"у зойках гудків, і у хвилях Дніпра,\n"
"і в хмарах отих пурпурових,\n"
"в грому канонад, що розвіяли в прах\n"
"чужинців в зелених мундирах,\n"
"в багнетах, що в тьмі пробивали нам шлях\n"
"до весен і світлих, і щирих.\n"
"Юначе! Хай буде для неї твій сміх,\n"
"і сльози, і все до загину...\n"
"Не можна любити народів других,\n"
"коли ти не любиш Вкраїну!..\n"
"Дівчино! Як небо її голубе,\n"
"люби її кожну хвилину.\n"
"Коханий любить не захоче тебе,\n"
"коли ти не любиш Вкраїну...\n"
"Любіть у труді, у коханні, у бою,\n"
"як пісню, що лине зорею…\n"
"Всім серцем любіть Україну свою —\n"
"і вічні ми будемо з нею!";

const int PARITY_EVEN = 0b0;
const int PARITY_ODD = 0b1;

const int MAX_EVEN_QUATRAIN_COUNT = 2;
const int MAX_ODD_QUATRAIN_COUNT = 2;

const int QUEUE_SIZE = 1 << 14; //16384 bytes 1 << 14
const int MAX_MSG_SIZE = 1 << 10; // 1024 bytes
const int STEP_SIZE = 4; // for quatrains step size = 4

const int NSEMS = 2; // semaphores for limit count of message in queue

int finishedWorkers = 0; // stop condition for receiver

union semun {
    int              val;    /* Value for SETVAL */
    struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
    unsigned short  *array;  /* Array for GETALL, SETALL */
    struct seminfo  *__buf;  /* Buffer for IPC_INFO
                                (Linux-specific) */
};

struct message {
    long mtype;       /* message type, must be > 0 */
    char mtext[MAX_MSG_SIZE];    /* message data */
};

void splitString(string _str, vector<string> *_data) {
    int offset = 0;
    for (int i = 0; i < _str.length(); i++) {
        if (_str[i] == 10) {
            _data->push_back(_str.substr(offset, i-offset));
            offset = i+1;
        }
    }
    _data->push_back(_str.substr(offset, _str.length()));    
}

int initSemaphore(int _semid, int _semnum, int _semval) {
    union semun semopts;
    semopts.val = _semval;

    return semctl(_semid, _semnum, SETVAL, semopts);
}

// worker process
int worker(int _parity, int _semid, int _msgqid) {
    // Enumerate verse quatrain
    vector<string> separatedVerse;
    splitString(verse, &separatedVerse);

    // start offset
    int offset = _parity*STEP_SIZE;

    message msg = {};
    sembuf semdown = {(ushort)_parity, -1, 0};

    // msgtype = 1 if odd and = 2 if even
    msg.mtype = (!_parity) + 1;

    // calculate iteration count (even always >= then odd because start with even)
    int partCount = separatedVerse.size()/(2*STEP_SIZE) + (!_parity ? separatedVerse.size()%(2*STEP_SIZE) != 0 : 0);

    for (int i = 0; i < partCount; i++) {
        string res = "";

        // quatrain assemble
        for (int j = offset + i*STEP_SIZE*2; j < offset + i*STEP_SIZE*2 + STEP_SIZE; j++) {
            res += separatedVerse[j] + "\n";
        }

        // sending quatrain
        strncpy(msg.mtext, res.c_str(), MAX_MSG_SIZE); 
        semop(_semid, &semdown, 1);
        msgsnd(_msgqid, &msg, res.length()+1, 0);
    }

    // sending zero-length message for signal that worker end
    strncpy(msg.mtext, "", MAX_MSG_SIZE);
    msgsnd(_msgqid, &msg, 0, 0);

    return 0;
}

int main() {
    //
    // Init semaphores
    //

    int semid = semget(getpid(), NSEMS, 0600 | IPC_CREAT);

    // Init semaphore for limit max count even quatrain of verse
    if (initSemaphore(semid, 0, MAX_EVEN_QUATRAIN_COUNT) != 0) {
        cout << "Error occured while init semaphore 0" << endl;
        exit(errno);
    }

    // Init semaphore for limit max count odd quatrain of verse
    if (initSemaphore(semid, 1, MAX_ODD_QUATRAIN_COUNT) != 0) {
        cout << "Error occured while init semaphore 1" << endl;
        exit(errno);
    }


    //
    // Init message queue
    //

    int msgqid = msgget(getpid(), 0600 | IPC_CREAT);

    // Queue config
    msqid_ds queueConfig;
    queueConfig.msg_qbytes = QUEUE_SIZE;
    queueConfig.msg_perm.uid = getuid();
    queueConfig.msg_perm.gid = getgid();
    queueConfig.msg_perm.mode = 0600;

    // Configurate message queue
    if (msgctl(msgqid, IPC_SET, &queueConfig) != 0) {
        cout << "Error occured while init queue size" << endl;
        exit(errno);
    }

    // Create workers
    if (fork() == 0) { // Even worker
        return worker(PARITY_EVEN, semid, msgqid);
    }

    if (fork() == 0) { // Odd worker
        return worker(PARITY_ODD, semid, msgqid);
    }

    // Receiver variables
    string result = "";
    message msg = {};
    ushort quatrainParity = 0;
    sembuf semFree = {0, 1, 0};

    while (finishedWorkers < 2) {
        // set quatrain parity to get part
        semFree.sem_num = quatrainParity;

        // receive quatrain
        int bytesCount = msgrcv(msgqid, &msg, MAX_MSG_SIZE, !quatrainParity+1, 0);
        
        // change quatrain parity
        quatrainParity = !quatrainParity;

        // check if worker end
        if (bytesCount == 0) {
            finishedWorkers++;
            continue;
        }

        // concat result and free queue
        result += string(msg.mtext);
        semop(semid, &semFree, 1);
    }

    cout << result << endl;
    return 0;
}