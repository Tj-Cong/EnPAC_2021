//
// Created by lewis on 19-7-19.
//

#include<iostream>
#include"Petri_Net.h"
#include <signal.h>
#include <unistd.h>
#include <csetjmp>

using namespace std;

/***********Global Functions***********/

void handler(int num) {
    //printf("time out .\n");
    //exit(0);
    longjmp(petrienv,1);
}

/*unsigned int BKDRHash(string str);
 * function: get a string's hash value
 * BKDR: HashValue(str) = sum(str[i]*seed^(length-i-1))
 * in: string str
 * out: unsigned int hash value
 * */
unsigned int BKDRHash(string str) {
    unsigned int seed = 31;
    unsigned int hash = 0;
    int length = str.length();
    int i = 0;
    for (i; i < length; i++) {
        hash = hash * seed + str[i];
    }
    hash = hash * seed;
    //保证最高位为0，这样在赋值给int型变量时，不用进行符号转换
    //hash = hash & 0x7fffffff;
    return hash;
}

void intersection(const vector<Small_Arc> &t1pre, const vector<Small_Arc> &t2pre, vector<int> &secidx) {
    secidx.clear();
    vector<Small_Arc>::const_iterator it1;
    vector<Small_Arc>::const_iterator it2;
    for (it1 = t1pre.begin(); it1 != t1pre.end(); ++it1) {
        for (it2 = t2pre.begin(); it2 != t2pre.end(); ++it2) {
            if (it1->idx == it2->idx)
                secidx.push_back(it1->idx);
        }
    }
}

/*int my_atoi(string str)
 * function: 将字符串转换为整型数据
 * in:string str, 由数字组成的字符串
 * out：int型数据
 * attention：该函数应当只能转换正整数，
 * 对于负数和小数，函数应当报错提示
 * */
int my_atoi(string str) {
    int len = str.length();
    int i;
    int result = 0;
    for (i = 0; i < len; i++) {
        if (str[i] >= '0' && str[i] <= '9') {
            result = result * 10 + (str[i] - '0');
        } else {
            cerr << "my_atoi error! The string contains illegal characters!" << endl;
            return -1;
        }
    }
    return result;
}

unsigned int stringToNum(const string &str) {
    istringstream iss(str);
    unsigned int num;
    iss >> num;
    return num;
}

/*****************Petri***************/
/*构造函数
 * */
Petri::Petri() {
    place = NULL;
    transition = NULL;
    arc = NULL;
    unittable = NULL;
    placecount = 0;
    transitioncount = 0;
    arccount = 0;
    NUPN = false;
    SAFE = false;
    PINVAR = false;
}

/*析构函数
 * */
Petri::~Petri() {
    delete [] place;
    delete [] transition;
    //delete[] arc;
    if (NUPN) {
        delete [] placeExtra;
        delete [] unittable;
    }
    if(PINVAR) {
        destroyPINVAR();
        delete [] pinvarExtra;
        delete [] weightsum0;
     }
}

void Petri::getSize(char *filename) {
    //读取文档
    TiXmlDocument *mydoc = new TiXmlDocument(filename);
    if (!mydoc->LoadFile()) {
        cerr << mydoc->ErrorDesc() << endl;
    }

    //获得根元素
    TiXmlElement *root = mydoc->RootElement();
    if (root == NULL) {
        cerr << "Failed to load file: no root element!" << endl;
        mydoc->Clear();
    }
    string rootvalue = root->Value();

    TiXmlElement *net = root->FirstChildElement("net");
    TiXmlElement *page = net->FirstChildElement("page");

    //遍历每一页
    while (page) {
        TiXmlElement *pageElement = page->FirstChildElement();
        while (pageElement) {
            string value = pageElement->Value();
            if (value == "place") {
                placecount++;
            } else if (value == "transition") {
                transitioncount++;
            } else if (value == "arc") {
                arccount++;
            }
            else if (value == "toolspecific") {
                NUPN = true;
                SAFE = false;
                TiXmlElement *PTsize = pageElement->FirstChildElement("size");
                TiXmlAttribute *attr = PTsize->FirstAttribute();
                placecount = stringToNum(attr->Value());
                attr = attr->Next();
                transitioncount = stringToNum(attr->Value());
                attr = attr->Next();
                arccount = stringToNum(attr->Value());

                TiXmlElement *structure = pageElement->FirstChildElement("structure");
                TiXmlAttribute *usize = structure->FirstAttribute();
                unitcount = stringToNum(usize->Value());

                TiXmlAttribute *safe_attr = usize->Next()->Next();
                string safe = safe_attr->Value();
                if(safe == "false") {
                    NUPN = false;
                    SAFE = true;
                    break;
                }
                allocHashTable();    //申请空间

                preNUPN(structure);
                delete mydoc;
                return;
            }
            pageElement = pageElement->NextSiblingElement();
        }
        page = page->NextSiblingElement();
    }

    allocHashTable();
    delete mydoc;
}

/*void Petri::preNUPN(TiXmlElement *structure)
 * function: 读取NUPN的预处理；解析每一个单元，读取每一个单元中的
 * localplace, 然后按顺序存入place库所表中（该函数中没有该操作，
 * 只是将它在库所表中的索引位置存入Directory中）
 */
void Petri::preNUPN(TiXmlElement *structure) {
    TiXmlElement *unit = structure->FirstChildElement("unit");
    unsigned short ut_iter = 0;
    index_t pptr = 0;

    //读取每一个unit
    while (unit) {
        //获取unit的id
        TiXmlAttribute *uid = unit->FirstAttribute();
        unittable[ut_iter].uid = uid->Value();
        unittable[ut_iter].startpos = pptr;

        //get local places in this unit
        TiXmlElement *localplace = unit->FirstChildElement("places");
        //get local places string
        const char *test = localplace->GetText();
        if (test == NULL) {
            unit = unit->NextSiblingElement();
            unittable[ut_iter].size = 0;
            ut_iter++;
            continue;
        }
        string placestr = test;

        placestr = placestr+" ";
        string singleplace;
        NUM_t localplaces_count = 0;
        int startpos=0,endpos=0;
        char HT=9,space=32,CR=13;
        while(endpos<placestr.length()) {
            if(placestr[endpos]==HT || placestr[endpos]==space || placestr[endpos]==CR)
            {
                singleplace = placestr.substr(startpos,endpos-startpos);

                place[pptr].myunit = ut_iter;
                place[pptr].myoffset = localplaces_count;
                place[pptr].id = singleplace;
                mapPlace.insert(pair<string,index_t>(singleplace,pptr));

                localplaces_count++;
                ++pptr;
                startpos = ++endpos;
            } else {
                endpos++;
            }
        }
        unittable[ut_iter].size = localplaces_count;
        ut_iter++;
        unit = unit->NextSiblingElement();
    }

    if (ut_iter != unitcount)
        cerr << "Error! NUPN unitcount error!" << endl;
    if (pptr != placecount)
        cerr << "Error! NUPN placecount error!" << endl;
}

NUM_t Petri::getPlaceSize() const {
    return placecount;
}

/*void Petri::allocHashTable()
 * function: allocate space for place, transition and arc
 * */
void Petri::allocHashTable() {

    //allocate space for place, transition and arc
    place = new Place[placecount];
    transition = new Transition[transitioncount];
    arc = new Arc[arccount];

    if (NUPN) {
        unittable = new Unit[unitcount];
        placeExtra = new NUPN_extra[placecount];
    }
}

/*int Petri::getPPosition(string str)
 * function: Given a string(a place's id), find its
 * index in place Hashtable. if couldn't find, return -1;
 * in:string str, place's id
 * out:place's index
 * */
index_t Petri::getPPosition(string str) {
    map<string,index_t>::iterator it;
    it=mapPlace.find(str);
    if(it==mapPlace.end()) {
        return INDEX_ERROR;
    }
    else {
        return it->second;
    }
}

/*int Petri::getTPosition(string str)
 * function: Given a string(a transition's id), find its
 * index in transition Hashtable. if couldn't find, return -1;
 * in:string str, transition's id
 * out:transition's index
 * */
index_t Petri::getTPosition(string str) {
    map<string,index_t>::iterator it;
    it=mapTransition.find(str);
    if(it==mapTransition.end()) {
        return INDEX_ERROR;
    }
    else {
        return it->second;
    }
}

index_t Petri::getPosition(string str, bool &isplace) {
    map<string,index_t>::iterator it;
    it=mapPlace.find(str);
    if(it!=mapPlace.end()) {
        isplace = true;
        return it->second;
    }

    it=mapTransition.find(str);
    if(it!=mapTransition.end()) {
        isplace = false;
        return it->second;
    }
    return INDEX_ERROR;
}

void Petri::readNUPN(char *filename) {
    TiXmlDocument *mydoc = new TiXmlDocument(filename);
    if (!mydoc->LoadFile()) {
        cerr << mydoc->ErrorDesc() << endl;
    }

    //获得根元素
    TiXmlElement *root = mydoc->RootElement();
    if (root == NULL) {
        cerr << "Failed to load file: no root element!" << endl;
        mydoc->Clear();
    }

    TiXmlElement *net = root->FirstChildElement("net");
    TiXmlElement *page = net->FirstChildElement("page");

    NUM_t tptr = 0;
    NUM_t aptr = 0;
    //遍历每一页
    while (page) {
        TiXmlElement *pageElement = page->FirstChildElement();
        while (pageElement) {
            string value = pageElement->Value();

            //库所
            if (value == "place") {
                Place p;
                TiXmlAttribute *id = pageElement->FirstAttribute();
                p.id = id->Value();
                TiXmlElement *initialmarking = pageElement->FirstChildElement("initialMarking");
                if (!initialmarking) {
                    p.initialMarking = 0;
                } else {
                    TiXmlElement *token = initialmarking->FirstChildElement("text");
                    if (token) {
                        string marking = token->GetText();
                        p.initialMarking = stringToNum(marking);
                    }
                }
                //
                index_t P_pidx = getPPosition(p.id);
                if (P_pidx == INDEX_ERROR) {
                    cerr << "getPPosition Error! Cannot find place!" << p.id << endl;
                    exit(0);
                }
                if(place[P_pidx].id != p.id) {
                    cerr << "mapPlace Error! place-id \""<<p.id<<"\" doesn't match!" << endl;
                    exit(0);
                }
                place[P_pidx].initialMarking = p.initialMarking;
            }
                //变迁
            else if (value == "transition") {
                //get information of Transition t;
                Transition &t = transition[tptr];
                TiXmlAttribute *id = pageElement->FirstAttribute();
                t.id = id->Value();

                mapTransition.insert(pair<string,index_t>(t.id,tptr));

                tptr++;

            }
                //弧
            else if (value == "arc") {
                Arc a;
                TiXmlAttribute *attr = pageElement->FirstAttribute();
                a.id = attr->Value();
                attr = attr->Next();
                a.source_id = attr->Value();
                attr = attr->Next();
                a.target_id = attr->Value();

                TiXmlElement *inscription = pageElement->FirstChildElement("inscription");
                if (!inscription) {
                    a.weight = 1;
                } else {
                    TiXmlElement *text = inscription->FirstChildElement("text");
                    if (text) {
                        string weight = text->GetText();
                        a.weight = stringToNum(weight);
                    }
                }
                arc[aptr++] = a;
            }
            pageElement = pageElement->NextSiblingElement();
        }
        page = page->NextSiblingElement();
    }

    //construct a petri net
    bool issourcePlace;
    int i = 0;
    int end = arccount;
    for (i; i < end; i++) {
        index_t index = getPosition(arc[i].source_id, issourcePlace);
        if (index != INDEX_ERROR) {
            arc[i].isp2t = issourcePlace;
            arc[i].source_idx = index;
            if (issourcePlace) {
                //该弧的前继节点是库所
                index_t tidx = getTPosition(arc[i].target_id);
                arc[i].target_idx = tidx;
                if (tidx != INDEX_ERROR) {
                    SArc place_post, tranx_pre;
                    place_post.weight = tranx_pre.weight = arc[i].weight;
                    place_post.idx = tidx;
                    tranx_pre.idx = index;
                    place[index].consumer.push_back(place_post);
                    transition[tidx].producer.push_back(tranx_pre);
                }
            } else {
                //该弧的前继节点是变迁
                index_t pidx = getPPosition(arc[i].target_id);
                arc[i].target_idx = pidx;
                if (pidx != INDEX_ERROR) {
                    SArc tranx_post, place_pre;
                    tranx_post.weight = place_pre.weight = arc[i].weight;
                    tranx_post.idx = pidx;
                    place_pre.idx = index;
                    place[pidx].producer.push_back(place_pre);
                    transition[index].consumer.push_back(tranx_post);
                }
            }
        }
    }

    computeUnitMarkLen();
    delete mydoc;
}

/*void Petri::readPNML(char *filename)
 * fucntion: Parse a PNML file, get its all
 * places, transitions and arcs, store them
 * in corresponding hashtables. After getting
 * them, construct a petri net(connect places
 * and arcs according to arcs)
 * */
void Petri::readPNML(char *filename) {
    //parse PNML
    //读取文档
    TiXmlDocument *mydoc = new TiXmlDocument(filename);
    if (!mydoc->LoadFile()) {
        cerr << mydoc->ErrorDesc() << endl;
    }

    //获得根元素
    TiXmlElement *root = mydoc->RootElement();
    if (root == NULL) {
        cerr << "Failed to load file: no root element!" << endl;
        mydoc->Clear();
    }

    TiXmlElement *net = root->FirstChildElement("net");
    TiXmlElement *page = net->FirstChildElement("page");

    index_t pptr = 0;
    index_t tptr = 0;
    index_t aptr = 0;
    //遍历每一页
    while (page) {
        TiXmlElement *pageElement = page->FirstChildElement();
        while (pageElement) {
            string value = pageElement->Value();

            //库所
            if (value == "place") {
                //获取库所信息
                Place &p = place[pptr];
                TiXmlAttribute *id = pageElement->FirstAttribute();
                p.id = id->Value();
                TiXmlElement *initialmarking = pageElement->FirstChildElement("initialMarking");
                if (!initialmarking) {
                    p.initialMarking = 0;
                } else {
                    TiXmlElement *token = initialmarking->FirstChildElement("text");
                    if (token) {
                        string marking = token->GetText();
                        unsigned int initm = stringToNum(marking);
                        if (initm > 65535) {
//                            cout << "CANNOT_COMPUTE" << endl;
//                            exit(0);
                            LONGBITPLACE = true;
                        }
                        p.initialMarking = stringToNum(marking);
                    }
                }

                //将库所放入库所表中，并维护Directory
                mapPlace.insert(pair<string,index_t>(p.id,pptr));
                ++pptr;
            }
                //变迁
            else if (value == "transition") {
                //获取变迁信息
                Transition &t=transition[tptr];
                TiXmlAttribute *id = pageElement->FirstAttribute();
                t.id = id->Value();

                //将变迁放入变迁表中，并维护Directory
                mapTransition.insert(pair<string,index_t>(t.id,tptr));
                ++tptr;
            }
                //弧
            else if (value == "arc") {
                Arc a;
                TiXmlAttribute *attr = pageElement->FirstAttribute();
                a.id = attr->Value();
                attr = attr->Next();
                a.source_id = attr->Value();
                attr = attr->Next();
                a.target_id = attr->Value();

                TiXmlElement *inscription = pageElement->FirstChildElement("inscription");
                if (!inscription) {
                    a.weight = 1;
                } else {
                    TiXmlElement *text = inscription->FirstChildElement("text");
                    if (text) {
                        string weight = text->GetText();
                        a.weight = stringToNum(weight);
                    }
                }
                arc[aptr++] = a;
            }
            pageElement = pageElement->NextSiblingElement();
        }
        page = page->NextSiblingElement();
    }

    //construct a petri net
    bool issourcePlace;
    int i = 0;
    int end = arccount;
    for (i; i < end; i++) {
        int index = getPosition(arc[i].source_id, issourcePlace);
        if (index != INDEX_ERROR) {
            arc[i].isp2t = issourcePlace;
            arc[i].source_idx = index;
            if (issourcePlace) {
                //该弧的前继节点是库所
                int tidx = getTPosition(arc[i].target_id);
                arc[i].target_idx = tidx;
                if (tidx != INDEX_ERROR) {
                    SArc place_post, tranx_pre;
                    place_post.weight = tranx_pre.weight = arc[i].weight;
                    place_post.idx = tidx;
                    tranx_pre.idx = index;
                    place[index].consumer.push_back(place_post);
                    transition[tidx].producer.push_back(tranx_pre);
                }
            } else {
                //该弧的前继节点是变迁
                int pidx = getPPosition(arc[i].target_id);
                arc[i].target_idx = pidx;
                if (pidx != INDEX_ERROR) {
                    SArc tranx_post, place_pre;
                    tranx_post.weight = place_pre.weight = arc[i].weight;
                    tranx_post.idx = pidx;
                    place_pre.idx = index;
                    place[pidx].producer.push_back(place_pre);
                    transition[index].consumer.push_back(tranx_post);
                }
            }
        }
    }
    delete mydoc;
}

//计算每一个单元的marking长度
void Petri::computeUnitMarkLen() {
    index_t i = 0;
    index_t temp_len = 0;

    unittable[0].mark_sp = 0;
    unittable[0].mark_length = ceil(log2(unittable[i].size + 1));
    temp_len = unittable[0].mark_sp + unittable[0].mark_length;

    for (i = 1; i < unitcount; i++) {
        unittable[i].mark_sp = temp_len;
        unittable[i].mark_length = ceil(log2(unittable[i].size + 1));
        temp_len = temp_len + unittable[i].mark_length;
    }

    //compute every place's patterns
    for(int i=0;i<placecount;++i) {
        NUPN_extra &pe = placeExtra[i];
        const Unit &uu = unittable[place[i].myunit];
        pe.intnum = uu.mark_sp / 32;
        pe.intoffset = uu.mark_sp % 32;
        if(pe.intoffset+uu.mark_length > 32)
            pe.cutoff = true;
        pe.low_read_mask = (((unsigned int)1<<uu.mark_length)-1) << pe.intoffset;
        pe.low_zero_mask = ~(pe.low_read_mask);
        pe.low_write_mask = (place[i].myoffset+1) << pe.intoffset;

        if(pe.cutoff) {
            pe.high_read_mask = (((unsigned int)1<<uu.mark_length)-1) >> (32-pe.intoffset);
            pe.high_zero_mask = ~(pe.high_read_mask);
            pe.high_write_mask = (place[i].myoffset+1) >> (32-pe.intoffset);
        }
    }
}


//判断当前网是否为SAFE网
void Petri::judgeSAFE() {
    if (NUPN || LONGBITPLACE) {
        SAFE = false;
        return;
    }
    char filename[] = "GenericPropertiesVerdict.xml";

    for (int i = 0; i < arccount; ++i) {
        if (arc[i].weight > 1) {
            SAFE = false;
            return;
        }
    }

    ifstream infile(filename, ios::in);
    if (!infile) {
        SAFE = false;
        return;
    }

    TiXmlDocument *mydoc = new TiXmlDocument(filename);
    if (!mydoc->LoadFile()) {
        cerr << mydoc->ErrorDesc() << endl;
    }

    //获得根元素
    TiXmlElement *root = mydoc->RootElement();
    if (root == NULL) {
        cerr << "Failed to load file: no root element!" << endl;
        mydoc->Clear();
    }

    TiXmlElement *verdict = root->FirstChildElement("verdict");
    while (verdict) {
        TiXmlAttribute *attr = verdict->FirstAttribute();
        attr = attr->Next();
        string value = attr->Value();
        if (value == "SAFE") {
            attr = attr->Next();
            string isafe = attr->Value();
            if (isafe == "true") {
                SAFE = true;
            } else {
                SAFE = false;
            }
            return;
        }
        verdict = verdict->NextSiblingElement();
    }
}

void Petri::checkarc() {
    for (int i = 0; i < arccount; i++) {
        if (arc[i].isp2t)  //place->transition
        {
            if (place[arc[i].source_idx].id != arc[i].source_id) {
                cerr << "Error! Arc Check Failed @" << arc[i].source_id << endl;
                exit(-1);
            }

            if (transition[arc[i].target_idx].id != arc[i].target_id) {
                cerr << "Error! Arc Check Failed @" << arc[i].target_id << endl;
                exit(-1);
            }
        } else {
            if (transition[arc[i].source_idx].id != arc[i].source_id) {
                cerr << "Error! Arc Check Failed @" << arc[i].source_id << endl;
                exit(-1);
            }

            if (place[arc[i].target_idx].id != arc[i].target_id) {
                cerr << "Error! Arc Check Failed @" << arc[i].target_id << endl;
                exit(-1);
            }
        }
    }
    cout << "Arc Check Passed! ^_^" << endl;
    delete [] arc;
}

/*void printPlace();
 * function:按以下格式打印出所有的库所：
 * Total places：
 *
 * id：
 * index:
 * hashvalue:
 * initialMarking:
 * producer:
 * consumer:
id:p19
index:0
hashvalue:0
initialMarking0
producer:t21
consumer:t13

 * */
void Petri::printPlace() {
    ofstream outplace("places.txt", ios::out);
    outplace << "Total places：" << placecount << endl;

    int i;
    for (i = 0; i < placecount; i++) {
        const Place &p = place[i];
        const NUPN_extra &pe = placeExtra[i];
        outplace << endl;
        outplace << i << endl;
        outplace << "id:" << p.id << endl;
        outplace << "initialMarking:" << p.initialMarking << endl;
        if(NUPN) {
            outplace << "myUnit:" << p.myunit << endl;
            outplace << "myOffset:" << p.myoffset << endl;
            outplace << "cutoff:" << (pe.cutoff?"TRUE":"FALSE") << endl;
            outplace << "intnum:" << pe.intnum << endl;
            outplace << "intoffset:" << pe.intoffset << endl;
            outplace << "low_read_mask:" <<hex<< pe.low_read_mask << endl;
            outplace << "low_zero_mask:" <<hex<< pe.low_zero_mask << endl;
            outplace << "low_write_mask:" <<hex<< pe.low_write_mask << endl;
            if(pe.cutoff) {
                outplace << "high_read_mask:" <<hex<< pe.high_read_mask << endl;
                outplace << "high_zero_mask:" <<hex<< pe.high_zero_mask << endl;
                outplace << "high_write_mask:" <<hex<< pe.high_write_mask << endl;
            }
        }
        outplace << "producer:";
        vector<SArc>::const_iterator iterp;
        for (iterp = p.producer.begin(); iterp != p.producer.end(); iterp++) {
            outplace << transition[iterp->idx].id << " ";
        }
        outplace << endl;
        outplace << "consumer:";

        vector<SArc>::const_iterator iterc;
        for (iterc = p.consumer.begin(); iterc != p.consumer.end(); iterc++) {
            outplace << transition[iterc->idx].id << " ";
        }
        outplace << endl;
    }
}

/*void printTransition()
 * function:打印出所有的变迁信息
 * 打印格式：
 * Total Transitions：
 *
 * id:
 * index:
 * hashvalue:
 * producer:
 * consumer:
 * */
void Petri::printTransition() {
    ofstream outTransition("transition.txt", ios::out);
    outTransition << "Total transitions:" << transitioncount << endl;

    int i;
    for (i = 0; i < transitioncount; i++) {
        Transition &t = transition[i];
        outTransition << endl;
        outTransition << i << endl;
        outTransition << "id:" << t.id << endl;

        outTransition << "producer:";
        vector<SArc>::iterator iterp;
        for (iterp = t.producer.begin(); iterp != t.producer.end(); iterp++) {
            outTransition << "(" << iterp->weight << "," << place[iterp->idx].id << ") ";
        }
        outTransition << endl;
        outTransition << "consumer:";

        vector<SArc>::iterator iterc;
        for (iterc = t.consumer.begin(); iterc != t.consumer.end(); iterc++) {
            outTransition << "(" << iterc->weight << "," << place[iterc->idx].id << ") ";
        }
        outTransition << endl;
    }

}

void Petri::printGraph() {
    ofstream outGraph("PetriNetGraph.dot", ios::out);
    outGraph << "digraph PetriNet{" << endl;
    //先定义库所
    outGraph << '\t' << "rankdir=LR" << endl;
    index_t i = 0;
    for (i = 0; i < placecount; i++) {
        outGraph << '\t' << place[i].id << " [shape=circle]" << endl;
    }

    //再定义变迁
    for (i = 0; i < transitioncount; i++) {
        outGraph << '\t' << transition[i].id << " [shape=box]" << endl;
    }

    outGraph << endl;

    //打印边关系
    for (i = 0; i < arccount; i++) {
        outGraph << '\t' << arc[i].source_id << "->" << arc[i].target_id << " [label=\"" << arc[i].weight << "\"]"<< endl;
    }
    outGraph << "}" << endl;
}

void Petri::printUnit() {
    index_t i = 0;
    ofstream outUnit("unit.txt", ios::out);

    outUnit << "单元总数：" << unitcount << endl;
    outUnit << endl;
    for (i; i < unitcount; i++) {
        outUnit << "id: " << unittable[i].uid << endl;
        outUnit << "size: " << unittable[i].size << endl;
        outUnit << "Mark_Length: " << unittable[i].mark_length << endl;
        outUnit << "mark_sp: " << unittable[i].mark_sp << endl;
        outUnit << "startpos: " << unittable[i].startpos << endl;
        outUnit << endl;
    }
}

void Petri::printTransition2CSV() {
    ofstream outTransition("transition.csv", ios::out);

    int i;
    for (i = 0; i < transitioncount; i++) {
        Transition &t = transition[i];
        outTransition << t.id << ",";

        vector<SArc>::iterator iterp;
        for (iterp = t.producer.begin(); iterp != t.producer.end(); iterp++) {
            outTransition <<place[iterp->idx].id << ",";
        }
        outTransition << "XXX,";
        vector<SArc>::iterator iterc;
        for (iterc = t.consumer.begin(); iterc != t.consumer.end(); iterc++) {
            outTransition << place[iterc->idx].id << ",";
        }
        outTransition << endl;
    }
}

//void Petri::computeDI() {
//    /*iterate over places*/
//    for(index_t i=0;i<placecount;++i) {
//        Place &p=place[i];
//        vector<SArc>::iterator coniter1,coniter2,proiter;
//        /*计算decreasing set*/
//        for(coniter1=p.consumer.begin();coniter1!=p.consumer.end();++coniter1) {
//            vector<SArc>::iterator backiter;
//            weight_t backweight;
//            for(backiter=transition[coniter1->idx].consumer.begin();backiter!=transition[coniter1->idx].consumer.end();++backiter) {
//                if(backiter->idx == i)
//                    break;
//            }
//            if(backiter == transition[coniter1->idx].consumer.end()) {
//                backweight = 0;
//            }
//            else {
//                backweight = backiter->weight;
//            }
//            /*如果送回这个库所的token数比取出来的多，就没有decrease关系*/
//            if(backweight >= coniter1->weight) {
//                continue;
//            }
//            for(coniter2=p.consumer.begin();coniter2!=p.consumer.end();++coniter2) {
//                if(backweight<coniter2->weight) {
//                    transition[coniter1->idx].decreasing.insert(coniter2->idx);
//                }
//            }
//        }
//        /*计算increasing set*/
//        for(proiter=p.producer.begin();proiter!=p.producer.end();++proiter) {
//            vector<SArc>::iterator backiter;
//            weight_t backweight;
//            for(backiter=p.consumer.begin();backiter!=p.consumer.end();++backiter) {
//                if(backiter->idx == proiter->idx)
//                    break;
//            }
//            if(backiter == p.consumer.end()) {
//                backweight = 0;
//            }
//            else {
//                backweight = backiter->weight;
//            }
//            if(backweight>=proiter->weight) {
//                continue;
//            }
//            for(coniter1=p.consumer.begin();coniter1!=p.consumer.end();++coniter1) {
//                transition[proiter->idx].increasing.insert(coniter1->idx);
//            }
//        }
//    }
//}

/* 计算得到P不变量 以及 重要库所
 * 得到关联矩阵
 * 将矩阵进行行阶梯转化
 * 其中的主元就是重要库所
 * 自由变量是非重要库所
 * P不变量 通过对行最简的矩阵 依次赋值可得
 * 对其中一个自由变量赋1 其余自由变量赋0
 * 求得主元的值 即为一个P不变量
 * （从矩阵的最后一行 依次往上求即可）
 * */
int Petri::computePinvariant() {

    float **matrix=NULL;
    short int **input=NULL;
    short int **output=NULL;
    ComputeRref *m=NULL;
    pinvarExtra = new Place_PINVAR_info[placecount];
    eq_var = new Equation_variables[placecount];

    signal(SIGALRM, handler);
    alarm(COMPUTE_P_INVAR_TIME);
    int r=setjmp(petrienv);

    if(r==0){
        //关联矩阵
        matrix = new float *[transitioncount];
        if(matrix == NULL)
            longjmp(petrienv,1);
        memset(matrix, 0, transitioncount * sizeof(float *));

        for (int i = 0; i < transitioncount; i++) {
            matrix[i] = new float[placecount];
            if(matrix[i] == NULL) {
                longjmp(petrienv,1);
            }
            memset(matrix[i], 0, placecount * sizeof(float));
        }

        //输入矩阵
        input = new short *[transitioncount];
        if(input == NULL) {
            longjmp(petrienv,1);
        }
        memset(input,0,transitioncount*sizeof(short *));
        for (int i = 0; i < transitioncount; i++) {
            input[i] = new short[placecount];
            if(input[i] == NULL) {
                longjmp(petrienv,1);
            }
            memset(input[i], 0, placecount * sizeof(short));
        }

        //输出矩阵
        output = new short *[transitioncount];
        if(output == NULL) {
            longjmp(petrienv,1);
        }
        memset(output,0,transitioncount*sizeof(short *));
        for (int i = 0; i < transitioncount; i++) {
            output[i] = new short[placecount];
            if(output[i] == NULL) {
                longjmp(petrienv,1);
            }
            memset(output[i], 0, placecount * sizeof(short));
        }

        //维护输出矩阵
        for (int i = 0; i < transitioncount; i++) {
            short tmp = transition[i].consumer.size();
            for (int j = 0; j < tmp; j++) {
                int row = i;
                int placeidx = transition[i].consumer[j].idx;
                int col = placeidx;
                short weight = transition[i].consumer[j].weight;
                output[row][col] = weight;
            }
        }

        //维护输入矩阵
        for (int i = 0; i < placecount; i++) {
            short tmp = place[i].consumer.size();
            for (int j = 0; j < tmp; j++) {
                int row = i;
                int tranidx = place[i].consumer[j].idx;
                int col = tranidx;
                short weight = place[i].consumer[j].weight;
                input[col][row] = weight;
            }
        }

        //A+ — A- = matrix
        for (int i = 0; i < transitioncount; i++) {
            for (int j = 0; j < placecount; j++) {
                matrix[i][j] = output[i][j] - input[i][j];
            }
        }

//        printIncidenceMatrix(matrix);

        //释放A+  A- 矩阵
        for (int i = 0; i < transitioncount; i++) {
            delete [] input[i];
            delete [] output[i];
        }
        delete [] input;
        delete [] output;
        MallocExtension::instance()->ReleaseFreeMemory();

        //计算行阶梯 得到的matrix是行阶梯形式的
        m = new ComputeRref(transitioncount, placecount, matrix);
        m->rref();
        delete m;
        m=NULL;

//        printIncidenceMatrix(matrix);

        //初始化
        for (int i = 0; i < placecount; i++) {
            eq_var[i].mainvar = false;
        }

        //判断秩 并且找出主元/自由变量
        RankOfmatrix = 0;
        for (int i = 0; i < transitioncount; i++) {
            for (int j = 0; j < placecount; j++) {
                if (fabs(matrix[i][j]) > 1e-6) {
                    RankOfmatrix++;
                    pinvarExtra[j].significant = true;
                    eq_var[j].mainvar = true;
                    break;
                }
            }
        }

        if ((double)RankOfmatrix / placecount > 0.9) {
            //释放关联矩阵
            for (int i = 0; i < transitioncount; i++) {
                delete [] matrix[i];
            }
            delete [] matrix;
            MallocExtension::instance()->ReleaseFreeMemory();
            return -1;
        }

        //初始化 保存P不变量的数组
        Pinvar = new float *[placecount - RankOfmatrix];
        if(Pinvar == NULL) {
            longjmp(petrienv,1);
        }
        memset(Pinvar,0,(placecount - RankOfmatrix)*sizeof(float *));

        for (int i = 0; i < placecount - RankOfmatrix; i++) {
            Pinvar[i] = new float[placecount];
            if(Pinvar[i] == NULL) {
                longjmp(petrienv,1);
            }
            memset(Pinvar[i], 0, placecount*sizeof(float));
        }

        /*对自由变量赋值 得到所有变量的值(求P不变量)
         * 自由变量的个数是n-r   (placecount-RankOfmatrix)
         * 依次对自由变量 一个赋1 其余自由变量赋0
         * */
        for (int i = 0; i < placecount - RankOfmatrix; i++) {
            short free_count = 0;
            //依次对自由变量 一个赋1 其余自由变量赋0
            for (int j = 0; j < placecount; j++) {
                if (free_count == i && !eq_var[j].mainvar) {
                    eq_var[j].variable = 1;
                    pinvarExtra[j].pinvarLink = i;
                    free_count++;
                } else if (!eq_var[j].mainvar) {
                    eq_var[j].variable = 0;
                    free_count++;
                }
            }

            /*从矩阵的底部开始 往上求主元的值
             *找到第一个非零的变量 对其求值
             * */
            for (int k = transitioncount - 1; k >= 0; k--) {
                for (int p = 0; p < placecount; p++) {
                    if (matrix[k][p] == 0)continue;
                    else //对第一个非0的变量 求得其值(此为主元)
                    {
                        //求得p到最后一个变量的和
                        float sum = 0.0;
                        for (int o = p + 1; o < placecount; o++) {
                            sum += matrix[k][o] * eq_var[o].variable;
                        }
                        eq_var[p].variable = (-sum) / matrix[k][p];
                        break;
                    }
                }
            }
            //将该P不变量写到二维数组中
            for (int j = 0; j < placecount; j++) {
                if (eq_var[j].variable == -0)
                    eq_var[j].variable = 0;
                Pinvar[i][j] = eq_var[j].variable;
            }
        }

//        printPinvar();

        //释放关联矩阵
        for (int i = 0; i < transitioncount; i++) {
            delete [] matrix[i];
        }
        delete [] matrix;
        MallocExtension::instance()->ReleaseFreeMemory();

        alarm(0);
        return 1;
    }
    else{
        if(!matrix)
        {
            //释放关联矩阵
            for (int i = 0; i < transitioncount; i++) {
                if(!matrix[i])
                    delete [] matrix[i];
            }
            delete [] matrix;
        }
        if(!input)
        {
            for (int i = 0; i < transitioncount; i++) {
                if(!input[i])
                    delete [] input[i];
            }
            delete [] input;
        }
        if(!output)
        {
            for (int i = 0; i < transitioncount; i++) {
                if(!output[i])
                    delete [] output[i];
            }
            delete [] output;
        }
        if(!m)
            delete m;
        MallocExtension::instance()->ReleaseFreeMemory();
        return -1;
    }
}

void Petri::printIncidenceMatrix(float **matrix) {
    /*test print*/
    cout << "placecount:" << placecount << endl;
    cout << "transitioncount:" << transitioncount << endl;
    cout << endl;
    for (int i = 0; i < transitioncount; i++) {
        for (int j = 0; j < placecount; j++) {
            printf("%8.3f", matrix[i][j]);
        }
        cout << endl;
    }

}

void Petri::printPinvar() {
    /*test print*/
    cout << "**********************************************" << endl;
    cout << "P-invar:" << endl;
    for (int i = 0; i < placecount - RankOfmatrix; i++) {
        for (int j = 0; j < placecount; j++)
            printf("%7.1f", Pinvar[i][j]);
        cout << endl;
    }
    cout << "**********************************************" << endl;
    cout << "RankOfmatrix:" << RankOfmatrix << endl;
}

void Petri::judgePINVAR() {
    if(NUPN || SAFE) {
        PINVAR = false;
        return;
    }
    if(computePinvariant() == 1) {
        PINVAR = true;
        computeBound();
        if(!PINVAR) {
            destroyPINVAR();
            delete [] pinvarExtra;
        }
    }
    else {
        PINVAR = false;
        delete [] pinvarExtra;
    }
    delete [] eq_var;
    MallocExtension::instance()->ReleaseFreeMemory();
}

void Petri::computeBound() {
    NUM_t *bound = new NUM_t [placecount];
    weightsum0 = new int[placecount - RankOfmatrix];
    for(int i=0;i<placecount;++i) {
        if(LONGBITPLACE)
            bound[i] = MAXUINT32;
        else
            bound[i] = MAXUNSHORT16;
    }
    memset(weightsum0,0,sizeof(int)*(placecount-RankOfmatrix));

    /*calculate i*m0*/
    for(int i=0;i<placecount-RankOfmatrix;++i) {
        float sum = 0;
        for(int j=0;j<placecount;++j) {
            sum += Pinvar[i][j] * place[j].initialMarking;
        }
        weightsum0[i] = sum>0?(int)(sum+0.5):(int)(sum-0.5);
    }

    /*筛选出semi-positive P不变量*/
    vector<int> semi_positive_index;
    for(int i = 0; i < placecount - RankOfmatrix; ++i) {
        bool posflag = true;
        for(int j=0;j<placecount;++j) {
            if(Pinvar[i][j] < 0) {
                posflag = false;
                break;
            }
        }
        if(posflag) {
            semi_positive_index.push_back(i);
        }
    }

    for(int i=0;i<semi_positive_index.size();++i) {
        for(int k=0; k<placecount; ++k) {
            if(Pinvar[semi_positive_index[i]][k]<1e-6 || pinvarExtra[k].significant==false)
                continue;
            else {
                double bb = ceil((double)weightsum0[semi_positive_index[i]]/Pinvar[semi_positive_index[i]][k]);
                if((NUM_t)bb<bound[k]) {
                    bound[k] = (NUM_t)bb;
                }
            }
        }
    }

    NUM_t bitcounter = 0;
    for(int j=0;j<placecount;++j) {
        Place_PINVAR_info &pinvarInfo = pinvarExtra[j];
        if(pinvarInfo.significant) {
            pinvarInfo.startpos = bitcounter;
            pinvarInfo.length = ceil(log2(bound[j]+1));
            pinvarInfo.intnum = pinvarInfo.startpos / 32;
            pinvarInfo.intoffset = pinvarInfo.startpos % 32;
            bitcounter += pinvarInfo.length;
            if(pinvarInfo.intoffset+pinvarInfo.length>32)
                pinvarInfo.cutoff = true;
        }
    }

    if((double)bitcounter/(16*placecount) > 0.8) {
        delete [] weightsum0;
        PINVAR = false;
    }
    delete [] bound;
    MallocExtension::instance()->ReleaseFreeMemory();
}

void Petri::destroyPINVAR() {
    for(int i=0;i<placecount-RankOfmatrix;++i) {
        if(!Pinvar[i])
            delete [] Pinvar[i];
    }
    delete [] Pinvar;
    MallocExtension::instance()->ReleaseFreeMemory();
}


