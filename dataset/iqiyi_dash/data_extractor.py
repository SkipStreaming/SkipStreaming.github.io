from json.decoder import JSONDecodeError
import os
import json
import MySQLdb

db = MySQLdb.connect('YOUR DB IP', 'YOUR DB USERNAME', 'YOUR DB PASSWORD', 'YOUR DB NAME', charset='utf8' )
cursor = db.cursor()

for root, dirs, files in os.walk('./data'):
    for filename in files:
        # print(dirs)
        file_path = root.replace('\\', '/') + '/' + filename
        # print(file_path)
        with open(file_path, 'r') as fp:
            try:
                json_data = json.load(fp)
            except Exception as e:
                print(e)
                continue
            bt = et = duration = -1
            if 'data' in json_data and 'p' in json_data['data'] and 'bt' in json_data['data']['p']:
                # print('bt: ' + str(json_data['data']['p']['bt']))
                bt = json_data['data']['p']['bt']

            if 'data' in json_data and 'p' in json_data['data'] and 'et' in json_data['data']['p']:
                # print('et: ' + str(json_data['data']['p']['et']))
                et = json_data['data']['p']['et']
            if 'data' in json_data and 'program' in json_data['data'] and 'video' in json_data['data']['program']:
                items = json_data['data']['program']['video']
                if len(items) > 0 and items[0]['duration']:
                    # print('duration: ' + str(items[0]['duration']))
                    duration = items[0]['duration']
            sql = "INSERT INTO iqiyi(video_name, episode, op, ed, duration) " \
                    "VALUES(\'%s\', %d, %s, %s, %s)" % \
                    (filename.split('_')[0],
                    int(filename.split('_')[1].split('.')[0]),
                    str(bt) if bt != -1 else 'NULL',
                    str(et) if et != -1 else 'NULL',
                    str(duration) if duration != -1 else 'NULL')
            # print(sql)
            try:
                cursor.execute(sql)
                db.commit()
            # except JSONDecodeError as e:
            #     print('json decode error')
            #     print(e)
            #     sql = "INSERT INTO iqiyi_marker_no_json_eps(video_name, episode) " \
            #           "VALUES(\'%s\', %d)" % \
            #           (filename.split('_')[0],
            #            int(filename.split('_')[1].split('.')[0]))
            #     # print(sql)
            #     cursor.execute(sql)
            #     db.commit()
            except Exception as e:
                print(e)
                print(sql)
                db.rollback()
            finally:
                fp.close()

