//
// Created by hemeng on 2025/3/29.
//
#include <at24c32.h>



static I2C_HandleTypeDef *eeprom_hi2c = NULL; // I2C句柄指针

// 初始化I2C句柄，传入空指针返回错误
HAL_StatusTypeDef EEPROM_Init(I2C_HandleTypeDef *hi2c) {
    if (hi2c == NULL) {
        return HAL_ERROR;
    }
    eeprom_hi2c = hi2c;
    return HAL_OK;
}

// 写入数据（自动处理页边界）
// data声明为const，确保数据内容不会被修改
HAL_StatusTypeDef EEPROM_Write(uint16_t memAddr, const uint8_t *data, const uint16_t size) {
    uint16_t bytesWritten = 0;
    while (bytesWritten < size) {
        // 计算当前页内的偏移和剩余空间
        const uint16_t pageOffset = memAddr % EEPROM_PAGE_SIZE;
        const uint16_t spaceInPage = EEPROM_PAGE_SIZE - pageOffset;
        const uint16_t chunkSize = (size - bytesWritten < spaceInPage) ? (size - bytesWritten) : spaceInPage;
        // 写入当前块数据
        HAL_StatusTypeDef status = HAL_I2C_Mem_Write(eeprom_hi2c, EEPROM_I2C_ADDR, memAddr,
                                                     I2C_MEMADD_SIZE_16BIT, (uint8_t *) &data[bytesWritten],
                                                     chunkSize, 100);
        if (status != HAL_OK) {
            return status;
        }
        // 等待EEPROM内部写入完成
        HAL_Delay(EEPROM_WRITE_DELAY);
        bytesWritten += chunkSize;
        memAddr += chunkSize;
    }
    return HAL_OK;
}

// 读取数据（无需分页）
HAL_StatusTypeDef EEPROM_Read(const uint16_t memAddr, uint8_t *data, const uint16_t size) {
    return HAL_I2C_Mem_Read(eeprom_hi2c, EEPROM_I2C_ADDR, memAddr,
                            I2C_MEMADD_SIZE_16BIT, data, size, 400);
}

#ifndef NO_EXAMPLES
void at24c32_example() {
    // 使用字符串常量，便于计算实际长度（包含结束符'\0'）
    const char *text =
            "这个故事里的主人公，或者叫主人公之一，叫萧子山。萧子山生在70年代的末期，是个普通人：家庭普通，相貌普通，天资很普通，而且不是个肯努力上进的人，所以读书很一般，受惠于大学的扩招，他也成了一名大学生。萧子山毕业以后，在珠三角的几家企业都呆过，上过黑心老板的当，做过不切实际的梦，最后好歹在一家外企的找了份待遇还不错的工作，勤勤恳恳的干活拿工资——这一干，就是差不多六七年，转眼三十了，住的依然是别人的屋子。平时没什么娱乐，就爱好历史，喜欢看点冷门的书。算是交过个女朋友，破了保留N久的处男之身。不过故事发生的时候，前女友的面容都快记不清了——他照旧属于要庆祝11.11的那伙人。我们故事的开头的这天晚上，天气很不好，黑漆漆的天空不断的在闪电——这样的日子持续了快半个月，有时候会在闪电之后下暴雨，有时候则会滚滚雷声，大地震的谣言传了很久,时间久了也慢慢习以为常了。萧子山打着哈欠从公交车上下来，眼皮浮肿，两腿酸软，头发蓬乱，一股穷忙族的邋遢派头。他已经一天一夜没回过家了。身为一个新晋的地区经理，为了迎接上级的检查，不得不在办公室里做了很久的报表，特别是那一笔又一笔的报销费用的去向，着实让他伤脑筋。要说萧某人是个企业里的蛀虫，那是天大的冤枉，萧子山正式当上这个职位，还不到三个月。进入这公司的六七年间，一直在地区销售代表这个基本职务上打转，上面的各级领导象韭菜一样割了一茬又一茬，他倒象韭菜根一样的存在着。三个月前，领导们又一次换班了，照例留下了一堆无法说明的报表和发票。只不过这次，他被指派当了地区经理。如果早上二年有这个职务……他肯定要对公司感激涕零，但是此时——萧子山只想对上面的领导们，无论是从前的还是现在的，说一声：草泥马。全球经济危机下的公司从去年开始就显得半身不遂，地区办公室里熟悉的同事，也一个接着一个地消失了。剩下的业务，他一个人干还显得悠闲无比。此时这个任命，升职后丝毫也不提调整那多年未动的工资，不给任何的开展业务指示。凭他七八年来的职业经验也不难明白——这是准备撤销地区办公室的前兆。他只不过是一留守人员，等到一切事务处理完毕，就得卷铺盖。但是文件还是得做……为了那点多年不动的工资还能多领几个月。萧子山第二十次的叹气。他提了个大旅行包，从包本身到包里的东西都是办公室里历年留存下来的促销赠品：从厨房围裙、汗衫、牙刷到圆珠笔无所不有。都是些粗劣的小商品。除了少数东西，他基本上都用不到。却还是很贪心的拿了回来——人穷志短——这话放在他身上真是再合适也不过了。出租房里还算干净整齐，冲个澡之后，精神反而亢奋起来了，随手打开电脑，上了网络。奥逊·威尔斯说，彩票是穷人的止痛药，那么网络就是萧子山这一类人的鸦片。无论是网游还是BBS，再或者他每天都要看的网络小说。萧子山没工作前也算是半个文艺青年，杂七杂八的书囫囵吞枣地看了不少，也喜欢舞文弄墨，不过天赋有限，没吃上文字饭。上过几年班之后，文艺小说，不管如何的深刻或者有意义，被他彻底的驱逐出去了——社会的一切现实，他看得太多了，不需要再靠小说来给他增加什么感想。他最爱看的，就是各式各样的穿越到过去再造历史的YY小说……这大概和他作为一个历史爱好者，总有一种期望篡改历史的想法有关。总之每天都得看上点，正如他自嘲的说过，这是“精神自慰”。";
    // 计算字符串长度（包括结束符'\0'）
    const uint16_t dataLength = strlen(text) + 1;
    const uint16_t eepromAddr = 0x00; // 测试地址
    uint8_t readData[dataLength];
    if (EEPROM_Write(eepromAddr, (const uint8_t *) text, dataLength) != HAL_OK) {
        printf("EEPROM Write Error\n");
    } else {
        printf("EEPROM Write Success\n");
    }

    if (EEPROM_Read(eepromAddr, readData, dataLength) != HAL_OK) {
        printf("EEPROM Read Error\n");
    } else {
        printf("Read ASCII: %s\r\n", (char *) readData);
    }
}
#endif
