import os
import json
import MySQLdb

db = MySQLdb.connect('YOUR DB IP', 'YOUR DB USERNAME', 'YOUR DB PASSWORD', 'YOUR DB NAME', charset='utf8' )
cursor = db.cursor()

for root,dirs,files in os.walk('./data'):
    for file in files:
        filename = os.path.join(root, file)
        with open(filename,'r',encoding='utf8') as fp:
            try:
                json_data = json.load(fp)
                # print (json_data)

                title = json_data['data']['data']['show']['title']
                stage = json_data['data']['data']['show']['stage']
                seconds = json_data['data']['data']['video']['seconds']
                head = None if 'head' not in json_data['data']['data']['dvd'].keys() else json_data['data']['data']['dvd']['head']
                tail = None if 'tail' not in json_data['data']['data']['dvd'].keys() else json_data['data']['data']['dvd']['tail']

                recap = None
                tailad = None

                if 'point' in json_data['data']['data']['dvd'].keys():
                    points = json_data['data']['data']['dvd']['point']
                    for p in points:
                        if p['ctype'] == 'recap':
                            recap = int(p['start'])
                        elif p['ctype'] == 'tailad':
                            tailad = int(p['start'])

                sql = """INSERT INTO youku(title, stage, seconds, head, tail, recap, tailad)
                    VALUES ('%s', %d, %f, %s, %s, %s, %s)""" % (title, stage, seconds,
                    'NULL' if head is None else str(head), 'NULL' if tail is None else str(tail),
                    'NULL' if recap is None else str(recap), 'NULL' if tailad is None else str(tailad))
                # print(sql)
            except Exception as e:
                print(e)
                continue
            
            try:
                cursor.execute(sql)
                db.commit()
            except Exception as e:
                print(e)
                db.rollback()
            else:
                # print()
                pass

