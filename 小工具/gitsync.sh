git pull &&
git add .. &&
git commit -m "$(date)" &&
git push &&
echo 'SUCCESS' &&
exit 0 ||
echo 'FAILED' &&
exit -1
