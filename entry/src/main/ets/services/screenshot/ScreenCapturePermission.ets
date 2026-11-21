// 截图与标题栏取色共用权限入口；申请授权本身不启动截图、不创建截图窗口。
import { abilityAccessCtrl, common, Permissions } from '@kit.AbilityKit';

export function hasScreenCapturePermission(context: common.Context): boolean {
  return abilityAccessCtrl.createAtManager().checkAccessTokenSync(context.applicationInfo.accessTokenId,
    'ohos.permission.CUSTOM_SCREEN_CAPTURE') === abilityAccessCtrl.GrantStatus.PERMISSION_GRANTED;
}

export async function requestScreenCapturePermission(context: common.UIAbilityContext): Promise<boolean> {
  if (hasScreenCapturePermission(context)) { return true; }
  const permissions: Permissions[] = ['ohos.permission.CUSTOM_SCREEN_CAPTURE'];
  const result = await abilityAccessCtrl.createAtManager().requestPermissionsFromUser(context, permissions);
  return result.authResults.length > 0 && result.authResults[0] === abilityAccessCtrl.GrantStatus.PERMISSION_GRANTED;
}
