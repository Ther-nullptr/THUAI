#include "AI.h"
#include "Constants.h"

//为假则play()调用期间游戏状态更新阻塞，为真则只保证当前游戏状态不会被状态更新函数与GameApi的方法同时访问
extern const bool asynchronous = true;

#include <random>
#include <iostream>
#include <ctime>
#include <chrono>
#include <string>
#include <cmath>

#define PI 3.1415926
#define CELL 1000 //网格尺寸

#define MOVETIME_WALLS 300 //躲墙的速度
#define MOVETIME_ENEMY 600 //躲敌人的速度
#define MOVETIME_ESCAPE 1000 //躲敌方子弹的速度
#define MOVETIME_RANDOM 400 //随机游走的速度
#define MOVETIME_BIRTHPOINTS 400 //躲出生点的速度
#define BULLET_ATTACK 700 //攻击时子弹的飞行速度
#define BULLET_COLOR 200 //染色时子弹的飞行速度
#define BULLET_SPEED 6

#define BIRTHPOINT1 2 // 改变方向所用的参考值
#define BIRTHPOINT2 47 // 
#define MIN BIRTHPOINT1 // 
#define MAX BIRTHPOINT2 // 
#define MONKEYX 2500
#define MONKEYY 3500

#define MOVEMODE1//这里可以改变行走模式(MOVEMODE1,MOVEMODE2)
#define ATTACKMODE1

/* 请于 VS2019 项目属性中开启 C++17 标准：/std:c++17 */

extern const THUAI4::JobType playerJob = THUAI4::JobType::Job6; //选手职业，选手 !!必须!! 定义此变量来选择职业

namespace
{
	[[maybe_unused]] std::uniform_real_distribution<double> direction(0, 2 * 3.1415926);
	[[maybe_unused]] std::default_random_engine e{ std::random_device{}() };
}

//控制游戏进度的一些常量
static auto sec = std::chrono::duration_cast<std::chrono::seconds>
(std::chrono::system_clock::now().time_since_epoch()).count();// 计时用
static bool startFlag = 1;//判定游戏是否为初始状态
static unsigned int directionFlag = 1;//控制人物移动方向优先级的标志值
static unsigned int propFlag = 0;
static bool move_to_left = 0;//控制人物是否优先向左移动
static bool move_to_up = 0;//控制人物是否优先向上移动
static unsigned int directionValue = 0;

//个人信息
static uint16_t myID;//ID
static uint32_t myGUID;//GUID
uint16_t lifeNum;//生命数 //TODO
uint32_t hp;//健康值 //TODO
uint16_t bulletNum;//子弹数
uint32_t selfPositionX;
uint32_t selfPositionY;//自身坐标
uint32_t attackforce;//自身攻击力//TODO
uint32_t movespeed;//自身移动速度//TODO
static THUAI4::ColorType color;//返回本队的颜色
bool isdying;//人物是否死亡

//其他玩家信息
uint32_t enemyPositionX, enemyPositionY, enemyHP, teammatePositionX, teammatePositionY;
double shootDirection, shootDistance,teammateDirection;

//子弹信息
uint32_t bulletPositionX, bulletPositionY;
double bulletDirection, characterBulletDirection,characterBulletDistance;

//地图信息
uint32_t cellX, cellY, wallX, wallY, birthpointX, birthpointY, myteamCellX, myteamCellY;
THUAI4::ColorType cellColor;
static bool wallinfo[50][50] = { 0 };//用数组存储墙的信息

//分数信息
uint32_t score;

#ifdef MOVEMODE2
enum class mydirection
{
	right,
	down,
	left,
	up,
	leftup,
	leftdown,
	rightup,
	rightdown
};
static mydirection d = mydirection::right;
#endif

//计算自身和目标点的角度
inline double getDirection(uint32_t selfPoisitionX, uint32_t selfPoisitionY, uint32_t aimPositionX, uint32_t aimPositionY)//获取角度的内联函数
{
	double delta_x = double(aimPositionX) - double(selfPoisitionX);
	double delta_y = double(aimPositionY) - double(selfPoisitionY);
	double direction = atan2(delta_y, delta_x);
	if (direction < 0)
	{
		direction = 2 * PI + direction;
	}
	return direction;
}

inline double getDistance(uint32_t selfPoisitionX, uint32_t selfPoisitionY, uint32_t aimPositionX, uint32_t aimPositionY)
{
	return sqrt((aimPositionX - selfPoisitionX) * (aimPositionX - selfPoisitionX) + (aimPositionY - selfPoisitionY) * (aimPositionY - selfPoisitionY));
}

//将绝对坐标转化为方格坐标
inline uint32_t getCellPosition(uint32_t t)
{
	return t / CELL;
}

//去向目标点
inline void gotoxy(GameApi& g, uint32_t selfPoisitionX, uint32_t selfPoisitionY, uint32_t aimPositionX, uint32_t aimPositionY, uint32_t movespeed)
{
	double direction = getDirection(selfPoisitionX, selfPoisitionY, aimPositionX, aimPositionY);
	double distance = getDistance(selfPoisitionX, selfPoisitionY, aimPositionX, aimPositionY);
	g.MovePlayer(1000 * distance / movespeed, direction);
}

//计算两个坐标是否属于同一个cell
inline bool areSameCell(uint32_t n1, uint32_t n2)
{
	bool condition1 = ((int(n1) - int(n2)) < 1000);
	bool condition2 = ((int(n2) - int(n1)) < 1000);
	return (condition1 && condition2);
}

//计算两个坐标是否相邻.
inline bool areAdjacentCell(uint32_t n1, uint32_t n2)
{
	bool condition1 = ((int(n1) - int(n2)) <= 1000);
	bool condition2 = ((int(n1) - int(n2)) > 0);
	return (condition1 && condition2);
}

//在遍历方格坐标时,可能会出现数组越界的情况,所以有必要对此进行检验
inline int getValidPosition(int t)
{
	if (t < 0)
		return 0;
	else if (t > 49)
		return 49;
	else
		return t;
}

//向道具移动的指令
inline void gotoProps(GameApi& g, bool wallinfo[50][50], uint32_t selfPositionX, uint32_t selfPositionY, uint32_t PropX, uint32_t PropY)
{
	bool moveflag = 1;
	uint32_t maxX = (selfPositionX < PropX ? PropX : selfPositionX);
	uint32_t maxY = (selfPositionY < PropY ? PropY : selfPositionY);
	uint32_t minX = (selfPositionX > PropX ? PropX : selfPositionX);
	uint32_t minY = (selfPositionY > PropY ? PropY : selfPositionY);
	for (int i = getCellPosition(minX); i <= getCellPosition(maxX); i++)
	{
		for (int j = getCellPosition(minY); j <= getCellPosition(maxY); j++)
		{
			if (wallinfo[i][j] == 1)
			{
				moveflag = 0;
				goto label;
			}
		}
	}
	if (moveflag)
	{
		gotoxy(g, selfPositionX, selfPositionY, PropX, PropY,movespeed);
	}
label:;
}
//当没有墙时,按照下面的指令进行移动

#ifdef MOVEMODE1
inline void moveWithoutWalls(GameApi& g, bool move_to_left, bool move_to_up, unsigned int directionFlag)
{

	if (move_to_left && move_to_up)
	{
		directionFlag < 5 ? g.MoveLeft(MOVETIME_RANDOM) : g.MoveUp(MOVETIME_RANDOM);
	}
	else if (move_to_left && !move_to_up)
	{
		directionFlag < 5 ? g.MoveLeft(MOVETIME_RANDOM) : g.MoveDown(MOVETIME_RANDOM);
	}
	else if (!move_to_left && move_to_up)
	{
		directionFlag < 5 ? g.MoveRight(MOVETIME_RANDOM) : g.MoveUp(MOVETIME_RANDOM);
	}
	else
	{
		directionFlag < 5 ? g.MoveRight(MOVETIME_RANDOM) : g.MoveDown(MOVETIME_RANDOM);
	}
}
#endif

#ifdef MOVEMODE2
inline void moveWithoutWalls(GameApi& g, mydirection d)
{
	switch (d)
	{
	case mydirection::right:
		g.MovePlayer(MOVETIME_RANDOM, 0.25 * PI);
		break;
	case mydirection::down:
		g.MovePlayer(MOVETIME_RANDOM, 1.75 * PI);
		break;
	case mydirection::left:
		g.MovePlayer(MOVETIME_RANDOM, 1.25 * PI);
		break;
	case mydirection::up:
		g.MovePlayer(MOVETIME_RANDOM, 0.75 * PI);
		break;
	}
}

#endif

void AI::play(GameApi& g)
{
	// 计时
	auto sec1 = std::chrono::duration_cast<std::chrono::seconds>
		(std::chrono::system_clock::now().time_since_epoch()).count();
	auto time = sec1 - sec;
	std::cout << time << std::endl;

	//这个时间条件方便将自己所控制的玩家移动到指定位置,实际应用时会删掉
	if (time > 15)
	{
		// 获取自身信息部分,供之后决策判断
		auto selfinfo = g.GetSelfInfo();
		myID = selfinfo->teamID;
		lifeNum = selfinfo->lifeNum;//获取生命数 //TODO
		hp = selfinfo->hp;//获取健康值 //TODO
		bulletNum = selfinfo->bulletNum;//获取子弹数
		selfPositionX = selfinfo->x;
		selfPositionY = selfinfo->y;//获取自身坐标
		isdying = selfinfo->isDying;
		attackforce = selfinfo->ap;
		movespeed = selfinfo->moveSpeed;
		myGUID = selfinfo->guid;
		auto myproptype = selfinfo->propType;

		auto color = g.GetSelfTeamColor();//返回本队的颜色

		// 便于监测,后续可以删掉
		std::cout << selfPositionX << ' ' << selfPositionY << std::endl;
		std::cout << bulletNum << std::endl;

		//出生点是一个比较尴尬的地方,会给判定造成"难以琢磨的问题",因此我们选择让玩家在出生后先离开出生点再进行其他判定操作
		if (startFlag)
		{
			if (getCellPosition(selfPositionX) == BIRTHPOINT1)
			{
				g.MoveDown(MOVETIME_BIRTHPOINTS);
			}
			else if (getCellPosition(selfPositionX) == BIRTHPOINT2)
			{
				g.MoveUp(MOVETIME_BIRTHPOINTS);
			}
			else if (getCellPosition(selfPositionY) == BIRTHPOINT1)
			{
				g.MoveRight(MOVETIME_BIRTHPOINTS);
			}
			else if (getCellPosition(selfPositionY) == BIRTHPOINT2)
			{
				g.MoveLeft(MOVETIME_BIRTHPOINTS);
			}
			startFlag = 0;
		}
		//如果玩家被击杀,将这几个标志值重置为默认状态
		if (isdying)
		{
			startFlag = 1;
			move_to_left = 0;
			move_to_up = 0;
		}

		// 获取周围道具的坐标,并移动(小步拿道具)
		auto props = g.GetProps();
		bool wallFlag = 1;
		if (props.size() != 0)
		{
			selfPositionX = selfinfo->x;
			selfPositionY = selfinfo->y;
			cellX = getCellPosition(selfPositionX);
			cellY = getCellPosition(selfPositionY);
			uint32_t propX = props[0]->x, propY = props[0]->y;// 先捡一个,不要贪多
			std::cout << propX << ' ' << propY << std::endl;
			if ((cellX == getCellPosition(propX)) && (cellY == getCellPosition(propY)))//如果就在脚下,直接捡起来就好了
			{
				THUAI4::PropType Type = props[0]->propType;//获取道具类型
				std::cout << int(Type) << std::endl;
				for (int i = 0; i < 20; i++)//尝试捡20次
				{
					g.Pick(props[0]->propType);
				}
			}
			else//附近有道具,进行移动
			{
				gotoProps(g, wallinfo, selfPositionX, selfPositionY, propX, propY);
			}
			switch (myproptype)
			{
			case THUAI4::PropType::Null:
				break;
			case THUAI4::PropType::Rice:
			case THUAI4::PropType::NegativeFeedback:
			case THUAI4::PropType::Totem:
			case THUAI4::PropType::Dirt:
			case THUAI4::PropType::Attenuator:
			case THUAI4::PropType::Divider:
				g.Use();
				break;
			default:
				auto propDirection = getDirection(selfPositionX, selfPositionY, MONKEYX, MONKEYY);
				auto propDistance = getDistance(selfPositionX, selfPositionY, MONKEYX, MONKEYY);
				g.Throw(propDistance / 8, propDirection);
				break;
			}
		}

		// 获取周围玩家的坐标,并做出决策
		auto characters = g.GetCharacters();
		if (characters.size() > 1)//如果周围除自己以外还有人
		{
			for (auto i = characters.begin(); i != characters.end(); i++)
			{
				selfPositionX = selfinfo->x;
				selfPositionY = selfinfo->y;
				if ((*i)->teamID != myID)//发现敌军
				{
					enemyPositionX = (*i)->x;
					enemyPositionY = (*i)->y;
					enemyHP = (*i)->hp;
					//g.Send(1, std::to_string(enemyPositionX) + "," + std::to_string(enemyPositionY) + "," + std::to_string(enemyHP));
					shootDirection = getDirection(selfPositionX, selfPositionY, enemyPositionX, enemyPositionY);
#ifdef ATTACKMODE1
					if (enemyHP <= 3 * attackforce)//残血敌人一击必杀
					{
						g.Attack(BULLET_ATTACK, shootDirection);//向指定方向发起进攻
						g.Attack(BULLET_ATTACK, shootDirection);//向指定方向发起进攻
						g.Attack(BULLET_ATTACK, shootDirection);//向指定方向发起进攻
					}
					else//如果不能一击必杀,就打消耗战
					{
						g.Attack(BULLET_ATTACK, shootDirection);//向指定方向发起进攻
					}
					g.MovePlayer(MOVETIME_ESCAPE / 4, shootDirection);//追着敌人打
					if (hp < 2000)//这种状态下可以判定AI即将被击杀,那么AI会将所有的子弹射出
					{
						//这段代码有风险!!
						while (bulletNum > 0)
						{
							g.Attack(BULLET_ATTACK, shootDirection);//向指定方向发起进攻
							bulletNum--;
						}
					}
#endif // ATTACKMODE1

#ifdef ATTACKMODE2
					for (int i = 0; i <= ememyHp / 1000; i++)
					{
						g.Attack(BULLET_ATTACK, shootDirection);
					}
#endif // ATTACKMODE2

				}
				else if ((*i)->guid != myGUID)//发现友军,也尽量远离,防止敌人一并歼灭或造成阻塞
				teammatePositionX = (*i)->x;
				teammatePositionY = (*i)->y;
				if (getDistance(selfPositionX, selfPositionY, teammatePositionX, teammatePositionY) <= 1.2 * CELL)
				{
					teammateDirection = getDirection(selfPositionX, selfPositionY, teammatePositionX, teammatePositionY);
					if (teammateDirection < PI)
					{
						g.MovePlayer(MOVETIME_ESCAPE, teammateDirection + PI);
					}
					else
					{
						g.MovePlayer(MOVETIME_ESCAPE, teammateDirection - PI);
					}
				}
			}
		}

		//获取周围子弹信息,并躲避子弹
		auto bullets = g.GetBullets();
		if (bullets.size() != 0)
		{
			for (auto i = bullets.begin(); i != bullets.end(); i++)
			{
				selfPositionX = selfinfo->x;
				selfPositionY = selfinfo->y;
				if ((*i)->teamID != myID)//发现敌方子弹
				{
					bulletPositionX = (*i)->x;
					bulletPositionY = (*i)->y;
					bulletDirection = (*i)->facingDirection;
					std::cout << bulletDirection << std::endl;
					characterBulletDirection = getDirection(selfPositionX, selfPositionY, bulletPositionX, bulletPositionY);//计算人与子弹的夹角
					characterBulletDistance = getDistance(selfPositionX, selfPositionY, bulletPositionX, bulletPositionY);//计算人与子弹的距离
					g.Attack(characterBulletDistance / BULLET_SPEED, characterBulletDirection);
					/*
					if (bulletDirection <= PI)//向垂直方向躲子弹
					{
						if (fabs(bulletDirection + PI - characterBulletDirection) <= PI * 0.1)
						{
							std::cout << "escape!" << std::endl;
							g.MovePlayer(MOVETIME_ESCAPE, PI / 2 + bulletDirection);
						}
					}
					else
					{
						if (fabs(bulletDirection - PI - characterBulletDirection) <= PI * 0.1)
						{
							std::cout << "escape!" << std::endl;
							g.MovePlayer(MOVETIME_ESCAPE, bulletDirection - PI * 0.5);
						}
					}
					*/
				}
			}
		}

		//判断颜色,并涂色,补充子弹
		selfPositionX = selfinfo->x;
		selfPositionY = selfinfo->y;
		cellX = getCellPosition(selfPositionX);
		cellY = getCellPosition(selfPositionY);
		bulletNum = selfinfo->bulletNum;
		int uncoloredplaces = 0;
		for (int i = getValidPosition(int(cellX - 2)); i <= getValidPosition(int(cellX + 2)); i++)
		{
			for (int j = getValidPosition(int(cellY - 2)); j <= getValidPosition(int(cellY + 2)); j++)
			{
				cellColor = g.GetCellColor(i, j);
				if (cellColor != color && wallinfo[i][j] != 1)//计数未被己方染色的地方(包括墙)
				{
					uncoloredplaces++;
				}
				if (cellColor == color)
				{
					myteamCellX = i;
					myteamCellY = j;
				}
			}
		}
		std::cout << uncoloredplaces << std::endl;
		if ((uncoloredplaces >= 20 && bulletNum >= 7) || (uncoloredplaces >= 15 && bulletNum >= 11)||bulletNum>=13)//对子弹数量和未染色区域进行权衡
		{
#ifdef MOVEMODE2 
			switch (d)
			{
			case mydirection::right:
				g.Attack(BULLET_COLOR, 0);
				break;
			case mydirection::down:
				g.Attack(BULLET_COLOR, 1.5 * PI);
				break;
			case mydirection::left:
				g.Attack(BULLET_COLOR, PI);
				break;
			case mydirection::up:
				g.Attack(BULLET_COLOR, 0.5 * PI);
				break;
			}
#endif

#ifdef MOVEMODE1
			g.Attack(1, direction(e));
#endif		
		}
		if (bulletNum < 4)//子弹不够,就去己方占领区补给
		{
			gotoProps(g, wallinfo,selfPositionX, selfPositionY, myteamCellX * CELL + 500, myteamCellY * CELL + 500);
		}


		// 获取周围墙的坐标,并移动
#ifdef MOVEMODE1
		auto walls = g.GetWalls();
		auto birthpoints = g.GetBirthPoints();//出生点也会阻塞玩家,与墙一视同仁
		if (walls.size() != 0 || birthpoints.size() != 0)
		{
			selfPositionX = selfinfo->x;
			selfPositionY = selfinfo->y;
			if (getCellPosition(selfPositionY) <= MIN)//如果到了边界区,则改变方向移动的优先级
			{
				move_to_left = 0;
			}
			if (getCellPosition(selfPositionY) >= MAX)
			{
				move_to_left = 1;
			}
			if (getCellPosition(selfPositionX) <= MIN)
			{
				move_to_up = 0;
			}
			if (getCellPosition(selfPositionX) >= MAX)
			{
				move_to_up = 1;
			}
			bool right = 1;
			bool left = 1;
			bool up = 1;
			bool down = 1;//标记特定方向上是否有墙的标志值
			for (auto i = walls.begin(); i != walls.end(); i++)
			{
				wallX = (*i)->x;
				wallY = (*i)->y;
				wallinfo[getCellPosition(wallX)][getCellPosition(wallY)] = 1;//将墙的信息写入数组
				if (left && areSameCell(selfPositionX, wallX) && areAdjacentCell(selfPositionY, wallY))
				{
					left = 0;
					move_to_left = 0;
				}
				if (right && areSameCell(selfPositionX, wallX) && areAdjacentCell(wallY, selfPositionY))
				{
					right = 0;
					move_to_left = 1;
				}
				if (up && areSameCell(selfPositionY, wallY) && areAdjacentCell(selfPositionX, wallX))
				{
					up = 0;
					move_to_up = 0;
				}
				if (down && areSameCell(selfPositionY, wallY) && areAdjacentCell(wallX, selfPositionX))
				{
					down = 0;
					move_to_up = 1;
				}
			}
			for (auto i = birthpoints.begin(); i != birthpoints.end(); i++)
			{
				birthpointX = (*i)->x;
				birthpointY = (*i)->y;
				wallinfo[getCellPosition(birthpointX)][getCellPosition(birthpointY)] = 1;//将墙的信息写入数组
				if (left && areSameCell(selfPositionX, birthpointX) && areAdjacentCell(selfPositionY, birthpointY))
				{
					left = 0;
					move_to_left = 0;
				}
				if (right && areSameCell(selfPositionX, birthpointX) && areAdjacentCell(birthpointY, selfPositionY))
				{
					right = 0;
					move_to_left = 1;
				}
				if (up && areSameCell(selfPositionY, birthpointY) && areAdjacentCell(selfPositionX, birthpointX))
				{
					up = 0;
					move_to_up = 0;
				}
				if (down && areSameCell(selfPositionY, birthpointY) && areAdjacentCell(birthpointX, selfPositionX))
				{
					down = 0;
					move_to_up = 1;
				}
			}
			std::cout << right << left << down << up << std::endl;
			directionValue = right + left + down + up;
			if (!move_to_left && !move_to_up)
			{
				if (directionFlag < 5)
				{
					if (right)
					{
						g.MoveRight(MOVETIME_WALLS);
					}
					else if (left)
					{
						g.MoveLeft(MOVETIME_WALLS);
					}
				}
				else
				{
					if (down)
					{
						g.MoveDown(MOVETIME_WALLS);
					}
					else if (up)
					{
						g.MoveUp(MOVETIME_WALLS);
					}
				}
			}
			if (move_to_left && !move_to_up)
			{
				if (directionFlag < 5)
				{
					if (down)
					{
						g.MoveDown(MOVETIME_WALLS);
					}
					else if (up)
					{
						g.MoveUp(MOVETIME_WALLS);
					}
				}
				else
				{
					if (left)
					{
						g.MoveLeft(MOVETIME_WALLS);
					}
					else if (right)
					{
						g.MoveRight(MOVETIME_WALLS);
					}
				}
			}
			if (!move_to_left && move_to_up)
			{
				if (directionFlag < 5)
				{
					if (up)
					{
						g.MoveUp(MOVETIME_WALLS);
					}
					else if (down)
					{
						g.MoveDown(MOVETIME_WALLS);
					}
				}
				else
				{
					if (right)
					{
						g.MoveRight(MOVETIME_WALLS);
					}
					else if (left)
					{
						g.MoveLeft(MOVETIME_WALLS);
					}
				}
			}
			if (move_to_left && move_to_up)
			{
				if (directionFlag < 5)
				{
					if (left)
					{
						g.MoveLeft(MOVETIME_WALLS);
					}
					else if (right)
					{
						g.MoveRight(MOVETIME_WALLS);
					}
				}
				else
				{
					if (up)
					{
						g.MoveUp(MOVETIME_WALLS);
					}
					else if (down)
					{
						g.MoveDown(MOVETIME_WALLS);
					}
				}
			}
		}
		else//没有墙时的移动策略
		{
			moveWithoutWalls(g, move_to_left, move_to_up, directionFlag);
		}
		directionFlag++;
		if (directionFlag == 10)//控制directionflag
		{
			directionFlag = 0;
		}

#endif

#ifdef MOVEMODE2

		auto walls = g.GetWalls();
		auto birthpoints = g.GetBirthPoints();//出生点也会阻塞玩家,与墙一视同仁
		if (walls.size() != 0 || birthpoints.size() != 0)
		{
			selfPositionX = selfinfo->x;
			selfPositionY = selfinfo->y;
			for (auto i = walls.begin(); i != walls.end(); i++)
			{
				wallX = (*i)->x;
				wallY = (*i)->y;
				wallinfo[getCellPosition(wallX)][getCellPosition(wallY)] = 1;
			}
			for (auto i = birthpoints.begin(); i != birthpoints.end(); i++)
			{
				birthpointX = (*i)->x;
				birthpointY = (*i)->y;
				wallinfo[getCellPosition(birthpointX)][getCellPosition(birthpointY)] = 1;
			}
			switch (d)
			{
			case mydirection::right:
			{
				g.MoveRight(MOVETIME_WALLS);
				std::cout << "right" << std::endl;
				for (auto i = selfPositionX - 0.5 * CELL; i <= selfPositionX + 0.5 * CELL; i += 0.5 * CELL)
				{
					if ((wallinfo[getCellPosition(i)][getCellPosition(selfPositionY) + 1] == 1) || getCellPosition(selfPositionY) >= MAX)
					{
						d = mydirection::down;
						break;
					}
				}
			}
			break;
			case mydirection::down:
			{
				std::cout << "down" << std::endl;
				g.MoveDown(MOVETIME_WALLS);
				for (auto i = selfPositionY - 0.5 * CELL; i <= selfPositionY + 0.5 * CELL; i += 0.5 * CELL)
				{
					if ((wallinfo[getCellPosition(selfPositionX) + 1][getCellPosition(i)] == 1) || getCellPosition(selfPositionX) >= MAX)
					{
						d = mydirection::left;
						break;
					}
				}
			}
			break;
			case mydirection::left:
			{
				std::cout << "left" << std::endl;
				g.MoveLeft(MOVETIME_WALLS);
				for (auto i = selfPositionX - 0.5 * CELL; i <= selfPositionX + 0.5 * CELL; i += 0.5 * CELL)
				{
					if ((wallinfo[getCellPosition(i)][getCellPosition(selfPositionY) - 1] == 1) || getCellPosition(selfPositionY) <= MIN)
					{
						d = mydirection::up;
						break;
					}
				}
			}
			break;
			case mydirection::up:
			{
				std::cout << "up" << std::endl;
				g.MoveUp(MOVETIME_WALLS);
				for (auto i = selfPositionY - 0.5 * CELL; i <= selfPositionY + 0.5 * CELL; i += 0.5 * CELL)
				{
					if ((wallinfo[getCellPosition(selfPositionX) - 1][getCellPosition(i)] == 1) || getCellPosition(selfPositionX) <= MAX)
					{
						d = mydirection::right;
						break;
					}
				}
			}
			break;
			}
		}
		else//没有墙时的移动策略
		{
			moveWithoutWalls(g, d);
		}
		directionFlag++;
		if (directionFlag == 10)//控制directionflag
		{
			directionFlag = 0;
		}

#endif MOVEMODE2

		//信息观测部分
		score = g.GetTeamScore();//返回本队当前分数
		std::cout << "score:" << score << std::endl;
	}

	//清屏,可选
	//system("cls");
}
