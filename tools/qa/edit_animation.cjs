const fs=require('fs');
const p='src/Render/Model/assimpmodelclass.cpp';
let s=fs.readFileSync(p,'utf8');
s=s.replace('    if (anim.duration <= 0.001f) return;', '    // A single-frame stance is a valid pose and must be sampled on every equip.');
s=s.replace('    animTimeTicks = fmodf(animTimeTicks, anim.duration);','    animTimeTicks = anim.duration > 0.001f ? fmodf(animTimeTicks, anim.duration) : 0.0f;');
s=s.replace('ReadNodeHierarchyToComponent(m_scene->mRootNode, XMMatrixIdentity(), anim, animTimeTicks, animComp.finalBoneMatrices);',`const AnimationClip* upper = GetAnimation(animComp.upperBodyAnimationIndex);
        float upperTicks = 0.0f;
        if (upper) {
            animComp.upperBodyTime += deltaTime;
            const float rate = upper->ticksPerSecond > 0.0f ? upper->ticksPerSecond : 24.0f;
            const float duration = upper->duration / rate;
            animComp.upperBodyTime = duration > 0.00001f ? fmodf(animComp.upperBodyTime, duration) : 0.0f;
            upperTicks = animComp.upperBodyTime * rate;
        }
        ReadNodeHierarchyToComponent(m_scene->mRootNode, XMMatrixIdentity(), anim,
            animTimeTicks, animComp.finalBoneMatrices, upper, upperTicks, animComp.upperBodyRoot);`);
const start=s.indexOf('void AssimpModelClass::ReadNodeHierarchyToComponent(');
let body=s.slice(start);
body=body.replace('std::vector<DirectX::XMFLOAT4X4>& outMatrices) const',`std::vector<DirectX::XMFLOAT4X4>& outMatrices,
    const AnimationClip* upperClip, float upperTime,
    const std::string& upperRoot, bool inUpperBody) const`);
body=body.replace('    for (const auto& ch : anim.channels) {',`    inUpperBody = inUpperBody || (upperClip && nodeName == NormalizeBoneName(upperRoot));
    const auto& sampledClip = inUpperBody && upperClip ? *upperClip : anim;
    const float sampleTime = inUpperBody && upperClip ? upperTime : animTime;
    for (const auto& ch : sampledClip.channels) {`);
for(const type of ['Position','Rotation','Scale'])body=body.replace('Interpolate'+type+'(channel->'+({Position:'positions',Rotation:'rotations',Scale:'scales'}[type])+', animTime)','Interpolate'+type+'(channel->'+({Position:'positions',Rotation:'rotations',Scale:'scales'}[type])+', sampleTime)');
body=body.replace('ReadNodeHierarchyToComponent(node->mChildren[i], globalTransform, anim, animTime, outMatrices);','ReadNodeHierarchyToComponent(node->mChildren[i], globalTransform, anim, animTime, outMatrices,\n            upperClip, upperTime, upperRoot, inUpperBody);');
s=s.slice(0,start)+body;fs.writeFileSync(p,s);
