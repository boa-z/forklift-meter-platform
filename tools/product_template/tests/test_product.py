"""验证最小合成帧通过真实 C adapter 写入产品 Domain。"""
import json,subprocess,sys
result=json.loads(subprocess.check_output([sys.argv[1]],input='F 0 0 200 0 8 6400000000000000\n',text=True))
assert result['signals']['vehicle.speed']['value']==10
assert result['signals']['vehicle.speed']['state']=='valid'
