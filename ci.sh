for i in /etc/profile.d/*.sh; do
  if [ -r $i ]; then
    . $i
  fi
done
module list
module spider
module avail
