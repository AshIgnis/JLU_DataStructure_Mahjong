// Mahjong 样例程序
// 贪(wu)心(nao)策略
// 作者：zhouhy
// 游戏信息：http://www.botzone.org/games#Mahjong
// 2014年7月19日更新

#include <iostream>
#include <string>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include "jsoncpp/json.h"

using namespace std;

int myID, cardCount, nonFrozenCount;
string cards[108];

// 对牌排序，获得可打出牌的数量
void SortAndGetFrozenCount()
{
	sort(cards, cards + cardCount);
	for (int i = 0; i < cardCount; i++)
	{
		if (cards[i][0] >= 'a')
		{
			nonFrozenCount = i;
			return;
		}
	}
	nonFrozenCount = cardCount;
}

// 递归检查能否胡
bool CheckHu(int num[10])
{
	for (int i = 1; i <= 9; i++)
		if (num[i] < 0)
			return false;
	if (num[0] == 0)
		return true;
	if (num[0] == 1)
		return false;
	if (num[0] == 2)
	{
		// 剩下两张将牌
		for (int i = 1; i <= 9; i++)
			if (num[i] == 2)
				return true;
		return false;
	}

	for (int i = 1; i <= 9; i++)
	{
		if (num[i] > 0) 
		{
			if (i <= 7)
			{
				// ABC型句子
				num[i]--, num[i + 1]--, num[i + 2]--;
				num[0] = num[0] - 3;
				if (CheckHu(num))
					return true;
				num[0] = num[0] + 3;
				num[i]++, num[i + 1]++, num[i + 2]++;
			}
			if (num[i] >= 3)
			{
				// AAA型句子
				num[i] = num[i] - 3;
				num[0] = num[0] - 3;
				if (CheckHu(num))
					return true;
				num[0] = num[0] + 3;
				num[i] = num[i] + 3;
			}
		}
	}

	return false;
}

// 判断能不能胡（未考虑碰过故意不杠的情况）
bool Hu()
{
	SortAndGetFrozenCount();
	if (cardCount < 14 || cardCount > 18)
		return false;
	if ((nonFrozenCount - 2) % 3 != 0)
		return false;
	int num[3][10] = { 0 }; // 顺序：万、筒、条，下标为0的项用于记录总数
	for (int i = 0; i < nonFrozenCount; i++)
	{
		// 没亮出来的手牌
		if (cards[i][0] == 'W')
		{
			num[0][cards[i][1] - '0']++;
			num[0][0]++;
		}
		if (cards[i][0] == 'B')
		{
			num[1][cards[i][1] - '0']++;
			num[1][0]++;
		}
		if (cards[i][0] == 'T')
		{
			num[2][cards[i][1] - '0']++;
			num[2][0]++;
		}
	}
	return CheckHu(num[0]) && CheckHu(num[1]) && CheckHu(num[2]);
}

int main()
{
	srand(time(0));

	// 读入JSON
	string str;
	getline(cin, str);
	Json::Reader reader;
	Json::Value input;
	reader.parse(str, input);

	// 分析自己收到的输入和自己过往的输出，并恢复状态
	int turnID = input["responses"].size();
	for (int i = 0; i < turnID; i++)
	{
		istringstream in(input["requests"][i].asString()),
			out(input["responses"][i].asString());
		string act;

		int type, who;
		string card, what;

		// 获得信息类型
		in >> type;
		switch (type)
		{
		case 0:
			// 告知编号
			in >> type;
			// response一定是PASS，不用看了
			break;
		case 1:
			// 告知手牌
			for (int j = 0; j < 13; j++)
				in >> cards[j];
			cardCount = 13;
			// response一定是PASS，不用看了
			break;
		case 2:
			// 摸牌，收入手牌
			in >> cards[cardCount++];

			// 然后我做了act
			out >> act;
			if (act == "PLAY")
			{
				// 当时我打出了……
				out >> act;
				// ……一张act！
				for (int j = 0; j < cardCount; j++)
				{
					if (cards[j] == act)
					{
						// 去掉这张牌，拿最后一张牌填这个空位
						cards[j] = cards[--cardCount];
						break;
					}
				}
			}
			else if (act == "GANG")
			{
				// 当时我杠了……
				out >> act;
				// 一张act！（act是大写的）
				// 在手牌里把这个牌变为小写（明示）
				for (int j = 0; j < cardCount; j++)
				{
					if (cards[j] == act)
						cards[j][0] += 'a' - 'A'; // 变成小写
				}
			}
			// HU不可能出现
			break;
		case 3:
			// 别人的动作
			in >> who >> what >> card;
			
			// 不是打牌的话，response一定是PASS，不用看了
			if (what != "PLAY")
				break;

			// 然后我又做了act
			out >> act;

			if (act == "PENG")
			{
				// 当时我碰牌了
				// 先看看这一回合有没有人胡
				{
					int tmp;
					string act, card2;
					istringstream nextInput(input["requests"][i + 1].asString());
					if (nextInput >> tmp >> tmp >> act >> card2 && act == "HU" && card == card2)
						break;
				}

				// 在手牌里把两张card变为小写（明示）
				int count = 0;
				for (int j = 0; j < cardCount; j++)
				{
					if (cards[j] == card) {
						cards[j][0] += 'a' - 'A'; // 变成小写
						if (++count == 2)
							break;
					}
				}
				// 再把card收入手牌
				card[0] += 'a' - 'A';
				cards[cardCount++] = card;

				// 然后我出了……
				out >> act;
				// ……一张act！
				for (int j = 0; j < cardCount; j++)
				{
					if (cards[j] == act)
					{
						// 去掉这张牌，拿最后一张牌填这个空位
						cards[j] = cards[--cardCount];
						break;
					}
				}
			}
			else if (act == "GANG")
			{
				// 当时我杠牌了
				// 先看看这一回合有没有人胡
				{
					int tmp;
					string act, card;
					istringstream nextInput(input["requests"][i + 1].asString());
					if (nextInput >> tmp >> tmp >> act >> card && act == "HU" && card != "SELF")
						break;
				}

				// 在手牌里把card都变为小写（明示）
				for (int j = 0; j < cardCount; j++)
				{
					if (cards[j] == card)
						cards[j][0] += 'a' - 'A'; // 变成小写
				}
				// 再把card收入手牌
				card[0] += 'a' - 'A';
				cards[cardCount++] = card;
			}
			// HU不可能出现，PASS不用处理
			break;
		case 4:
			// 这种情况不可能出现
			;
		}
	}

	// 看看自己本回合输入
	istringstream in(input["requests"][turnID].asString());
	int type, who;
	string act, card, temp, myAction = "PASS";
	in >> type;
	if (type == 2)
	{
		// 轮到我摸牌
		in >> card;

		// 能不能胡？
		cards[cardCount++] = card;
		if (Hu()) // 注意此时牌已经排序
			myAction = "HU";
		else
		{
			// 能不能杠？
			int count = 0;
			temp = card;
			temp[0] += 'a' - 'A';
			for (int i = 0; i < cardCount; i++)
			{
				if (cards[i] == temp || cards[i] == card)
					count++;
			}
			if (count == 4)
			{
				// 杠！
				myAction = "GANG " + card;
			}
			else
			{
				// 不能杠就算了……找一张牌出
				myAction = "PLAY " + cards[rand() % nonFrozenCount];
			}
		}
	}
	else if (type == 3)
	{
		// 其他玩家……
		in >> who >> act >> card;

		if (act == "PLAY") // 除非别人打牌，否则什么也干不了
		{
			// 先收进来

			// 提示：
			// 如果只能PASS，
			// 手牌也不用恢复，
			// 因为下次会重新计算
			cards[cardCount++] = card;
			// 能不能胡？
			if (Hu()) // 注意此时牌已经排序
				myAction = "HU";
			else
			{
				// 能不能杠/碰？
				int count = 0;
				temp = card;
				temp[0] += 'a' - 'A';
				for (int i = 0; i < cardCount; i++)
				{
					if (cards[i] == temp)
						count++;
					else if (cards[i] == card)
					{
						cards[i][0] += 'a' - 'A';
						count++;
					}
				}
				if (count == 4)
				{
					// 杠！
					myAction = "GANG";
				}
				else if (count == 3)
				{
					SortAndGetFrozenCount();
					// 碰！然后随便找一张牌出
					myAction = "PENG " + cards[rand() % nonFrozenCount];
				}
			}
		}
	}
	else if (type == 4)
	{
		// ……流局了TAT
		// 找一个目标牌型
		myAction = "W1 W1 W1 T1 T1 T1 B1 B1 B1 B2 B2 B2 T9 T9";
	}
	// 别的情况我只能PASS

	// 输出决策JSON
	Json::Value ret;
	ret["response"] = myAction;
	ret["data"] = "";
	Json::FastWriter writer;
	cout << writer.write(ret) << endl;
	return 0;
}