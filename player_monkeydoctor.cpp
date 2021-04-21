#include "AI.h"
#include "Constants.h"
#define PI  3.1415926
//为假则play()调用期间游戏状态更新阻塞，为真则只保证当前游戏状态不会被状态更新函数与GameApi的方法同时访问
extern const bool asynchronous = false;

#include <random>
#include <iostream>
#include <cmath>

/* 请于 VS2019 项目属性中开启 C++17 标准：/std:c++17 */

extern const THUAI4::JobType playerJob = THUAI4::JobType::Job4; //选手职业，选手 !!必须!! 定义此变量来选择职业

namespace
{
	[[maybe_unused]] std::uniform_real_distribution<double> direction(0, 2 * 3.1415926);
	[[maybe_unused]] std::default_random_engine e{ std::random_device{}() };
}

//个人信息
static uint16_t MmyID;//ID
uint16_t MlifeNum;//生命数 //TODO
uint32_t Mhp;//健康值 //TODO
uint16_t MbulletNum;//子弹数
uint32_t MselfPositionX;
uint32_t MselfPositionY;//自身坐标
double Mmovedirection;
bool Misdying;//人物是否死亡

//子弹信息
const uint32_t MbulletSpeed = 24;
uint32_t bulletPositionX;
uint32_t bulletPositionY;
double bulletDirection, characterBulletDirection;

//障碍物信息
uint32_t wallx;
uint32_t wally;

//计算角度函数
inline double getDirection(uint32_t selfPoisitionX, uint32_t selfPoisitionY, uint32_t aimPositionX, uint32_t aimPositionY)
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

void AI::play(GameApi& g)
{
	//个人信息获取
	auto self = g.GetSelfInfo();
	MbulletNum = self->bulletNum;
	MselfPositionX = self->x;
	MselfPositionY = self->y;
	MmyID = self->teamID;

	//队友发来的信息获取
	std::string buffer("empty");
	bool whethershoot = false;
	int rivalx = 0, rivaly= 0, rivalchp=0;
	if (g.MessageAvailable())
	{
		if (g.TryGetMessage(buffer))
		{
			int flag = 0;
			for (std::string::size_type i = 0; i != buffer.size(); i++)
			{
				if (buffer[i] == ',')
					flag++;
				else
				{
					if (flag == 0)
					{
						rivalx *= 10;
						rivalx += (buffer[i] - 48);
					}
					if (flag == 1)
					{
						rivaly *= 10;
						rivaly += (buffer[i] - 48);
					}
					if (flag == 2)
					{
						rivalchp *= 10;
						rivalchp += (buffer[i] - 48);
					}
				}
			}
			whethershoot = true;
		}
		else
		{
			std::cout << " cannot get message" << std::endl;
		}
	}
	//猴子的打击
	if (whethershoot)
	{
		double attackangle = getDirection(MselfPositionX, MselfPositionY, rivalx, rivaly);
		double attackdistance = getDistance(MselfPositionX, MselfPositionY, rivalx, rivaly);
		std::cout << attackangle <<' '<<attackdistance << std::endl;
		if (MbulletNum > 1 && rivalchp > 0)
		{
			g.Attack(attackdistance/MbulletSpeed, attackangle);
			//rivalchp -= 3000;
		}
	}
	//猴子躲避函数
	auto bullets = g.GetBullets();
	if (bullets.size() != 0)
	{
		for (auto i = bullets.begin(); i != bullets.end(); i++)
		{
			if ((*i)->teamID != MmyID)//发现敌方子弹
			{
				bulletPositionX = (*i)->x;
				bulletPositionY = (*i)->y;
				characterBulletDirection = getDirection(MselfPositionX, MselfPositionY, bulletPositionX, bulletPositionY);//计算人与子弹的夹角
				if (bulletDirection <= PI)//向垂直方向躲子弹
				{
					if (fabs(bulletDirection + PI - characterBulletDirection) <= PI * 0.1)
						g.MovePlayer(1000, PI / 2 + bulletDirection);
				}
				else
				{
					if (fabs(bulletDirection - PI - characterBulletDirection) <= PI * 0.1)
						g.MovePlayer(1000, bulletDirection - PI * 0.5);
				}
			}
		}
	}
	//遇到墙体时的移动
	auto walls = g.GetWalls();
	if (walls.size() != 0)
	{
		for (auto i = walls.begin(); i != walls.end(); i++)
		{
			wallx = (*i)->x;
			wally = (*i)->y;
			if (Mmovedirection == getDirection(MselfPositionX, MselfPositionY, wallx, wally))
			{
				Mmovedirection -= PI * 0.5;
				g.MovePlayer(1000, Mmovedirection);
			}
		}
	}
}
