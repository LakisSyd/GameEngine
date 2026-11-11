const fs=require('fs');let p='tools/export_kevin_weapon_ik.py',s=fs.readFileSync(p,'utf8');s=s.replace("print('NORMAL',factor,list(center))",`print('NORMAL',factor,list(center))
# EditBone matrices contain rotation and length, not the source object's scale.
# Match sampled matrices to this normalized rest skeleton.
for frames in samples.values():
    for poses in frames.values():
        for n,m in poses.items():
            loc,rot,_=(normal@m).decompose()
            poses[n]=Matrix.LocRotScale(loc,rot,Vector((1,1,1)))`);
s=s.replace('target=normal@poses[n]','target=poses[n]').replace("'parent_matrix':normal@poses[pb.parent.name]","'parent_matrix':poses[pb.parent.name]");fs.writeFileSync(p,s);
